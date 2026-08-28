#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include <QColor>
#include <QStringList>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::kLiveArray;
using SettingsUiInternal::kMaxArraySteps;
using SettingsUiInternal::kStepNudge;

bool SettingsUi::openArrayEditor(const QString& settingKey, QString* error)
{
    m_arrayKey = settingKey;
    m_arrayDraft = m_settings.dwellSequence;
    if (m_arrayDraft.isEmpty()) {
        m_arrayDraft = AppSettings::defaultDwellSequence();
    }
    refreshArrayEditor();
    notifyStatus(QStringLiteral("Edit dwell sequence"));
    return true;
}

PageDocument SettingsUi::buildArrayDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveArray);
    doc.name = QStringLiteral("Dwell sequence");
    const int n = qBound(1, m_arrayDraft.size(), kMaxArraySteps);
    initGrid(doc, 5, n + 2, 920, 200 + (n + 2) * 68, 10, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];

    const EditorSwatch sw = editorSwatch();
    const QColor decBg = sw.nudge;
    const QColor incBg = sw.nudge;
    const QColor editBg = sw.edit;
    const QColor delBg = sw.cancel;
    const QColor valBg = sw.value;

    for (int i = 0; i < n; ++i) {
        const int ms = m_arrayDraft[i];
        grid.cells.push_back(cell(QStringLiteral("dec_%1").arg(i), QStringLiteral("−"), i, 0,
                                  QStringLiteral("settings.array.nudge.%1.dec").arg(i), decBg));
        PageCell val = cell(QStringLiteral("val_%1").arg(i), QStringLiteral("%1 ms").arg(ms), i, 1,
                            {}, valBg, 1, false, QStringLiteral("value"));
        grid.cells.push_back(val);
        grid.cells.push_back(cell(QStringLiteral("inc_%1").arg(i), QStringLiteral("+"), i, 2,
                                  QStringLiteral("settings.array.nudge.%1.inc").arg(i), incBg));
        grid.cells.push_back(cell(QStringLiteral("edit_%1").arg(i), QStringLiteral("Edit"), i, 3,
                                  QStringLiteral("settings.array.edit.%1").arg(i), editBg));
        grid.cells.push_back(cell(QStringLiteral("del_%1").arg(i), QStringLiteral("Trash"), i, 4,
                                  QStringLiteral("settings.array.del.%1").arg(i), delBg));
    }

    const int bar = n;
    grid.cells.push_back(cell(QStringLiteral("decAll"), QStringLiteral("− all"), bar, 0,
                              QStringLiteral("settings.array.decAll"), sw.nudge));
    grid.cells.push_back(cell(QStringLiteral("reset"), QStringLiteral("Reset"), bar, 1,
                              QStringLiteral("settings.array.reset"), sw.warn));
    grid.cells.push_back(cell(QStringLiteral("incAll"), QStringLiteral("+ all"), bar, 2,
                              QStringLiteral("settings.array.incAll"), sw.nudge));
    grid.cells.push_back(cell(QStringLiteral("add"), QStringLiteral("Add"), bar, 3,
                              QStringLiteral("settings.array.add"), sw.add, 2));

    const int foot = n + 1;
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), foot, 0,
                              QStringLiteral("settings.array.save"), sw.save, 3));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), foot, 3,
                              QStringLiteral("settings.array.cancel"), sw.cancel, 2));
    return doc;
}

void SettingsUi::refreshArrayEditor()
{
    QString err;
    if (!presentLive(m_array, QLatin1String(kLiveArray), buildArrayDocument(), &err)) {
        notifyStatus(err);
    }
}

void SettingsUi::arrayNudge(int index, int dir)
{
    if (!m_array.active || index < 0 || index >= m_arrayDraft.size()) {
        return;
    }
    m_arrayDraft[index] = qBound(50, m_arrayDraft[index] + dir * kStepNudge, 10000);
    refreshArrayEditor();
}

void SettingsUi::arrayNudgeAll(int dir)
{
    if (!m_array.active) {
        return;
    }
    for (int& ms : m_arrayDraft) {
        ms = qBound(50, ms + dir * kStepNudge, 10000);
    }
    refreshArrayEditor();
}

void SettingsUi::arrayRemove(int index)
{
    if (!m_array.active || index < 0 || index >= m_arrayDraft.size()) {
        return;
    }
    if (m_arrayDraft.size() <= 1) {
        notifyStatus(QStringLiteral("Need at least one step"));
        return;
    }
    m_arrayDraft.removeAt(index);
    refreshArrayEditor();
}

void SettingsUi::arrayAdd()
{
    if (!m_array.active) {
        return;
    }
    if (m_arrayDraft.size() >= kMaxArraySteps) {
        notifyStatus(QStringLiteral("Maximum %1 steps").arg(kMaxArraySteps));
        return;
    }
    m_arrayDraft.push_back(m_arrayDraft.isEmpty() ? 700 : m_arrayDraft.last());
    refreshArrayEditor();
}

void SettingsUi::arrayReset()
{
    if (!m_array.active) {
        return;
    }
    m_arrayDraft = m_settings.dwellSequence;
    if (m_arrayDraft.isEmpty()) {
        m_arrayDraft = AppSettings::defaultDwellSequence();
    }
    refreshArrayEditor();
}

bool SettingsUi::arraySave(QString* error)
{
    if (!m_array.active) {
        if (error) {
            *error = QStringLiteral("Sequence editor is not open");
        }
        return false;
    }
    QStringList parts;
    for (int ms : m_arrayDraft) {
        parts << QString::number(ms);
    }
    QString err;
    if (!m_settings.applyNumericBuffer(QStringLiteral("dwellSequence"), parts.join(QLatin1Char(',')),
                                       &err)) {
        notifyStatus(err);
        if (error) {
            *error = err;
        }
        return false;
    }
    apply(true);
    m_arrayKey.clear();
    m_arrayDraft.clear();
    closeLive(m_array);
    notifyStatus(QStringLiteral("Saved dwell sequence = %1").arg(m_settings.dwellSequenceString()));
    return true;
}

bool SettingsUi::arrayCancel(QString* error)
{
    if (!m_array.active) {
        if (error) {
            *error = QStringLiteral("Sequence editor is not open");
        }
        return false;
    }
    m_arrayKey.clear();
    m_arrayDraft.clear();
    closeLive(m_array);
    notifyStatus(QStringLiteral("Sequence edit cancelled"));
    return true;
}

bool SettingsUi::arrayEditIndex(int index, QString* error)
{
    if (!m_array.active || index < 0 || index >= m_arrayDraft.size()) {
        if (error) {
            *error = QStringLiteral("Invalid sequence step");
        }
        return false;
    }
    m_numpadKey.clear();
    m_numpadTitle = QStringLiteral("Step %1").arg(index + 1);
    m_numpadHint = QStringLiteral("Dwell time for this step (ms).");
    m_numpadResetSeed = QString::number(m_arrayDraft[index]);
    m_numpadBuffer = m_numpadResetSeed;
    m_numpadReturn = NumpadReturn::Array;
    m_numpadArrayIndex = index;
    m_numpadColorChannel.clear();
    if (!presentNumpad(error)) {
        return false;
    }
    notifyStatus(QStringLiteral("Edit step %1").arg(index + 1));
    return true;
}

} // namespace gazer

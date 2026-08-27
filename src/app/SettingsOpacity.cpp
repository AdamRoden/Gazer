#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"

#include <QColor>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::kLiveOpacity;

QColor SettingsUi::flashOpacityPreview() const
{
    QColor c = m_settings.resolvedTheme().text;
    c.setAlpha(qBound(0, qRound(255.0 * double(m_opacityDraft) / 100.0), 255));
    return c;
}

bool SettingsUi::openFlashForeground(QString* error)
{
    if (!openOpacityEditor(error)) {
        return false;
    }
    if (!m_settings.flashUseForeground) {
        m_settings.flashUseForeground = true;
        m_opacitySetMode = true;
        apply(true);
    }
    return true;
}

bool SettingsUi::openOpacityEditor(QString* error)
{
    m_opacityDraft = qBound(0, m_settings.flashForegroundOpacity, 100);
    m_opacityRevert = m_opacityDraft;
    m_opacitySetMode = false;
    if (!presentLive(m_opacity, QLatin1String(kLiveOpacity), buildOpacityDocument(), error)) {
        m_opacity.reset();
        return false;
    }
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(flashOpacityPreview());
    }
    notifyStatus(QStringLiteral("Edit flash opacity"));
    return true;
}

PageDocument SettingsUi::buildOpacityDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveOpacity);
    doc.name = QStringLiteral("Flash opacity");
    initGrid(doc, 12, 3, 1100, 500, 8, 72, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();
    grid.cells.push_back(cell(QStringLiteral("edit_opacity"), QStringLiteral("Edit"), 0, 0,
                              QStringLiteral("settings.opacity.scrub"), pal.edit, 1, true, {}, {},
                              QStringLiteral("PhysicalKeys")));
    grid.cells.push_back(cell(QStringLiteral("dec_opacity"), QStringLiteral("−"), 0, 1,
                              QStringLiteral("settings.opacity.nudge.dec"), pal.nudge));
    grid.cells.push_back(cell(QStringLiteral("track_opacity"), QStringLiteral("Opacity"), 0, 2, {},
                              QColor(), 9, false, QStringLiteral("slider"),
                              QStringLiteral("opacity")));
    grid.cells.push_back(cell(QStringLiteral("inc_opacity"), QStringLiteral("+"), 0, 11,
                              QStringLiteral("settings.opacity.nudge.inc"), pal.nudge));
    grid.cells.push_back(cell(QStringLiteral("preview"), QStringLiteral("Preview"), 1, 0, {},
                              QColor(), 12, false, QStringLiteral("preview"),
                              QStringLiteral("%1%").arg(m_opacityDraft)));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 2, 0,
                              QStringLiteral("settings.opacity.save"), pal.save, 6));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 2, 6,
                              QStringLiteral("settings.opacity.cancel"), pal.cancel, 6));
    return doc;
}

void SettingsUi::refreshOpacityEditor()
{
    if (!m_opacity.active || m_numpad.active) {
        return;
    }
    QString err;
    if (!presentLive(m_opacity, QLatin1String(kLiveOpacity), buildOpacityDocument(), &err)) {
        notifyStatus(err);
        return;
    }
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(flashOpacityPreview());
    }
}

void SettingsUi::closeOpacityEditor()
{
    if (!m_opacity.active) {
        return;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    if (m_opacitySetMode) {
        m_settings.flashUseForeground = false;
        apply(true);
    }
    m_opacitySetMode = false;
    QString err;
    closeLive(m_opacity);
}

void SettingsUi::opacityNudge(int dir)
{
    if (!m_opacity.active) {
        return;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    m_opacityDraft = qBound(0, m_opacityDraft + dir, 100);
    refreshOpacityEditor();
}

bool SettingsUi::opacitySave(QString* error)
{
    if (!m_opacity.active) {
        if (error) {
            *error = QStringLiteral("Opacity editor is not open");
        }
        return false;
    }
    m_settings.flashUseForeground = true;
    m_settings.flashForegroundOpacity = m_opacityDraft;
    m_opacitySetMode = false;
    apply(true);
    const int pct = m_opacityDraft;
    closeOpacityEditor();
    notifyStatus(QStringLiteral("Flash opacity = %1%").arg(pct));
    return true;
}

} // namespace gazer

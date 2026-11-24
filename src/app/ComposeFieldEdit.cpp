#include "app/ComposeUi.h"

#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"

#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using compose_detail::addSettingsChrome;
using compose_detail::formColumnOrigin;
using compose_detail::formColumnWidth;
using compose_detail::formFieldRowHeight;
using compose_detail::kFormFieldRowTop;
using compose_detail::paintSave;
using compose_detail::paintSurface;

bool ComposeUi::fieldEditing() const
{
    return m_fieldEditor != FieldEditor::None;
}

bool ComposeUi::textFieldOpen() const
{
    return m_fieldEditor == FieldEditor::Name || m_fieldEditor == FieldEditor::Phrase;
}

bool ComposeUi::blockItemKeys() const
{
    return nameEditing() && !borrowingPhrase() && !textFieldOpen() && !editingFileName();
}

void ComposeUi::closeFieldEditor()
{
    m_fieldEditor = FieldEditor::None;
    m_fieldBuffer.clear();
    if (hasLive(kFieldLiveId)) {
        m_pages.closePage(QString(kFieldLiveId));
    }
}

void ComposeUi::rebuildFieldEditor()
{
    if (!fieldEditing() || !hasLive(kFieldLiveId)) {
        return;
    }
    QString err;
    PageDocument doc = buildFieldDocument();
    doc.id = QString(kFieldLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
}

void ComposeUi::focusFormField(const QString& which)
{
    if (!nameEditing()) {
        return;
    }
    FieldEditor next = FieldEditor::None;
    if (which == QLatin1String("name")) {
        next = FieldEditor::Name;
    } else if (which == QLatin1String("phrase")) {
        if (m_nameEdit != NameEditKind::Pin) {
            return;
        }
        next = FieldEditor::Phrase;
    } else {
        return;
    }
    if (m_fieldEditor == next && hasLive(kFieldLiveId)) {
        return;
    }
    if (fieldEditing()) {
        cancelFormEditor();
    }
    m_fieldEditor = next;
    if (next == FieldEditor::Name) {
        m_fieldBuffer.load(m_formName);
    } else {
        m_fieldBuffer.load(m_formPhrase);
    }
    QString err;
    if (!presentLive(QString(kFieldLiveId), buildFieldDocument(), &err)) {
        closeFieldEditor();
        if (!err.isEmpty()) {
            notify(err);
        }
        return;
    }
    if (isOpen()) {
        showComposeLayer(1);
    }
}

void ComposeUi::saveFormEditor()
{
    if (!fieldEditing()) {
        return;
    }
    if (m_fieldEditor == FieldEditor::Name) {
        m_formName = m_fieldBuffer.text();
    } else if (m_fieldEditor == FieldEditor::Phrase) {
        m_formPhrase = m_fieldBuffer.text();
        m_formPhraseTouched = true;
    }
    closeFieldEditor();
    rebuildForm();
}

void ComposeUi::cancelFormEditor()
{
    if (!fieldEditing() && !hasLive(kFieldLiveId)) {
        return;
    }
    closeFieldEditor();
    rebuildForm();
}

void ComposeUi::clearFormEditor()
{
    if (!textFieldOpen()) {
        return;
    }
    m_fieldBuffer.clear();
    rebuildFieldEditor();
}

bool ComposeUi::nudgeFieldCaret(const QString& command)
{
    if (!textFieldOpen()) {
        return false;
    }
    const int n = m_fieldBuffer.text().size();
    if (command == QLatin1String("home")) {
        m_fieldBuffer.setCaret(0);
    } else if (command == QLatin1String("end")) {
        m_fieldBuffer.setCaret(n);
    } else if (command == QLatin1String("arrowLeft")) {
        m_fieldBuffer.setCaret(m_fieldBuffer.caret() - 1);
    } else if (command == QLatin1String("arrowRight")) {
        m_fieldBuffer.setCaret(m_fieldBuffer.caret() + 1);
    } else {
        return false;
    }
    rebuildFieldEditor();
    return true;
}

PageDocument ComposeUi::buildFieldDocument() const
{
    PageDocument doc;
    doc.id = QString(kFieldLiveId);
    const bool phrase = m_fieldEditor == FieldEditor::Phrase;
    doc.name = phrase ? QStringLiteral("Phrase") : QStringLiteral("Name");
    addSettingsChrome(doc);
    const ThemeColors theme = m_settings.resolvedTheme();
    const PageDim board = speakBoardWidth();
    initTopOverlay(doc, 4, 1, {1.0}, 8, 0, theme, formFieldRowHeight(m_composeAuthored),
                   PageDim::expression(formColumnWidth(board)));
    PageGrid& grid = doc.grids[0];
    grid.offset.x = formColumnOrigin(phrase, board);
    grid.offset.y = PageDim::pixels(kFormFieldRowTop);
    grid.style.background.token = QStringLiteral("transparent");
    grid.style.thickness = PageBox::all(0);
    grid.style.radius = PageBox::all(0);
    grid.style.borderColor.reset();
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor field = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    grid.columnTracks = {PageTrackSize::starWeight(2.4), PageTrackSize::starWeight(1.0),
                         PageTrackSize::starWeight(1.0), PageTrackSize::starWeight(1.05)};

    PageCell box = cell(QStringLiteral("field_text"), m_fieldBuffer.text(), 0, 0, {}, field, 1,
                        QStringLiteral("display"));
    box.textStyle = QStringLiteral("row");
    box.style.radius = PageBox::all(10);
    box.style.borderColor = accent;
    box.style.thickness = PageBox::all(2);
    box.caretIndex = qBound(0, m_fieldBuffer.caret(), box.label.size());
    if (m_fieldBuffer.hasSelection()) {
        box.selectionStart = m_fieldBuffer.selectionStart();
        box.selectionEnd = m_fieldBuffer.selectionEnd();
    }
    grid.cells.push_back(std::move(box));

    PageCell clear = cell(QStringLiteral("field_clear"), QStringLiteral("Clear"), 0, 1,
                          QStringLiteral("compose.formField.clear"), key);
    clear.styleId = QStringLiteral("button");
    paintSurface(clear, key);
    clear.style.radius = PageBox::all(10);
    grid.cells.push_back(std::move(clear));
    PageCell save = cell(QStringLiteral("field_save"), QStringLiteral("Save"), 0, 2,
                         QStringLiteral("compose.formField.save"), accent);
    paintSave(save, accent);
    save.style.radius = PageBox::all(10);
    grid.cells.push_back(std::move(save));
    PageCell cancel = cell(QStringLiteral("field_cancel"), QStringLiteral("Cancel"), 0, 3,
                           QStringLiteral("compose.formField.cancel"), key);
    cancel.styleId = QStringLiteral("button");
    paintSurface(cancel, key);
    cancel.style.radius = PageBox::all(10);
    grid.cells.push_back(std::move(cancel));
    return doc;
}

} // namespace gazer

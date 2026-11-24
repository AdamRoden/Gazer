#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/SoundboardStore.h"
#include "layout/PageSession.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using SettingsPageBuild::makeNested;
using compose_detail::aboveKeyboardHeight;
using compose_detail::addSettingsChrome;
using compose_detail::kFormFieldRowGap;
using compose_detail::paintDanger;
using compose_detail::paintItemFace;
using compose_detail::paintSave;
using compose_detail::paintSurface;
using compose_detail::parseColor;
using compose_detail::trackPx;

namespace {

const SoundboardTopic* topicOwning(const SoundboardStore& board, const QString& buttonId)
{
    for (const SoundboardTopic& topic : board.topics()) {
        for (const SoundboardButton& button : topic.buttons) {
            if (button.id == buttonId) {
                return &topic;
            }
        }
    }
    return nullptr;
}

} // namespace

QString ComposeUi::formNameText() const
{
    return m_formName.trimmed();
}

QString ComposeUi::formPhraseText() const
{
    return m_formPhrase.trimmed();
}

int ComposeUi::formOccupiedSpan(bool rows) const
{
    int n = 1;
    const SoundboardTopic* topic = m_board.findTopic(m_editId);
    if (!topic) {
        return n;
    }
    for (const SoundboardButton& button : topic->buttons) {
        const int origin = rows ? button.row : button.col;
        const int span = qMax(1, rows ? button.rowSpan : button.colSpan);
        n = qMax(n, origin + span);
    }
    const int cap = rows ? SoundboardStore::kMaxRows : SoundboardStore::kMaxCols;
    return qBound(1, n, cap);
}

int ComposeUi::formItemCount() const
{
    if (m_nameEdit == NameEditKind::Topic) {
        return m_board.topics().size();
    }
    if (m_nameEdit == NameEditKind::Pin) {
        if (const SoundboardTopic* topic = topicOwning(m_board, m_editId)) {
            const int cols = qMax(1, topic->gridCols);
            int rows = qMax(1, topic->gridRows);
            if (rows < SoundboardStore::kMaxRows) {
                ++rows;
            }
            int cap = rows * cols;
            for (const SoundboardButton& button : topic->buttons) {
                cap = qMax(cap, button.row * cols + button.col + 1);
            }
            return cap;
        }
    }
    return 0;
}

int ComposeUi::formLiveIndex() const
{
    if (m_nameEdit == NameEditKind::Topic) {
        const QVector<SoundboardTopic>& topics = m_board.topics();
        for (int i = 0; i < topics.size(); ++i) {
            if (topics.at(i).id == m_editId) {
                return i;
            }
        }
        return -1;
    }
    if (m_nameEdit == NameEditKind::Pin) {
        for (const SoundboardTopic& topic : m_board.topics()) {
            for (const SoundboardButton& button : topic.buttons) {
                if (button.id == m_editId) {
                    const int cols = qMax(1, topic.gridCols);
                    return button.row * cols + button.col;
                }
            }
        }
    }
    return -1;
}

bool ComposeUi::applyFormDrafts(QString* error)
{
    // Spoken text only. Copying the label or source into utteranceText would
    // hide a clip, because a phrase wins over the clip when it plays.
    QString err;
    if (m_nameEdit == NameEditKind::Pin && m_formPhraseTouched) {
        (void)m_board.setButtonUtterance(m_editId, formPhraseText(), &err);
    }
    if (m_nameEdit == NameEditKind::Topic
        && !m_board.setTopicGridSize(m_editId, m_formCols, m_formRows, &err)) {
        if (error) {
            *error = err;
        }
        return false;
    }
    const int live = formLiveIndex();
    const int count = formItemCount();
    if (live < 0 || count <= 0) {
        return true;
    }
    const int want = qBound(0, m_formIndex, count - 1);
    const int delta = want - live;
    if (delta == 0) {
        return true;
    }
    bool ok = true;
    if (m_nameEdit == NameEditKind::Topic) {
        ok = m_board.moveTopic(m_editId, delta, &err);
    } else if (m_nameEdit == NameEditKind::Pin) {
        ok = m_board.placeButtonAt(m_editId, want, &err);
    }
    if (!ok && error) {
        *error = err;
    }
    return ok;
}

void ComposeUi::nudgeFormIndex(int delta)
{
    if (!nameEditing() || delta == 0) {
        return;
    }
    const int count = formItemCount();
    if (count <= 0) {
        return;
    }
    m_formIndex = qBound(0, m_formIndex + delta, count - 1);
    rebuildForm();
    rebuildFieldEditor();
}

void ComposeUi::nudgeFormCols(int delta)
{
    if (m_nameEdit != NameEditKind::Topic || delta == 0) {
        return;
    }
    const int minCols = formOccupiedSpan(false);
    m_formCols = qBound(minCols, m_formCols + delta, SoundboardStore::kMaxCols);
    rebuildForm();
    rebuildFieldEditor();
}

void ComposeUi::nudgeFormRows(int delta)
{
    if (m_nameEdit != NameEditKind::Topic || delta == 0) {
        return;
    }
    const int minRows = formOccupiedSpan(true);
    m_formRows = qBound(minRows, m_formRows + delta, SoundboardStore::kMaxRows);
    rebuildForm();
    rebuildFieldEditor();
}

void ComposeUi::rebuildForm()
{
    if (!nameEditing() || !hasLive(kFormLiveId)) {
        return;
    }
    QString err;
    PageDocument doc = buildFormDocument();
    doc.id = QString(kFormLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
}

PageDocument ComposeUi::buildFormDocument() const
{
    const bool topic = m_nameEdit == NameEditKind::Topic;
    PageDocument doc;
    doc.id = QString(kFormLiveId);
    doc.name = topic ? QStringLiteral("Edit Topic") : QStringLiteral("Edit Phrase");
    addSettingsChrome(doc);
    const ThemeColors theme = m_settings.resolvedTheme();
    initTopOverlay(doc, 1, 3, {1.0, 8.0, 1.2}, 8, 12, theme, aboveKeyboardHeight(m_composeAuthored),
                   speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.style.radius = PageBox::all(16);
    grid.rowTracks = {trackPx(72), PageTrackSize::starWeight(), trackPx(72)};
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor danger = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);

    auto rowLabel = [](const QString& id, const QString& label, const QString& note = {}) {
        PageCell text = cell(id, label, 0, 0, {}, QColor(), 1, QStringLiteral("label"), note);
        text.textStyle = QStringLiteral("row");
        text.styleId = QStringLiteral("plain");
        return text;
    };
    auto editIcon = [](const QString& id, const QString& command) {
        PageCell edit = cell(id, {}, 0, 1, command, QColor(), 1, {}, {}, QStringLiteral("editSquare"));
        edit.styleId = QStringLiteral("joinRight");
        return edit;
    };
    auto fieldRow = [&](PageGrid& host, int row, const QString& id, const QString& name,
                        const QString& value, const QString& icon, const QColor& valueBg,
                        const QString& command, const QString& note = {}) {
        PageGrid line = makeNested(id + QStringLiteral("_row"), row, 0, 1, 2, 6);
        line.styleId = QStringLiteral("group");
        line.marginPx = 8;
        line.cells.push_back(rowLabel(id + QStringLiteral("_caption"), name, note));
        PageGrid act = makeNested(id + QStringLiteral("_act"), 0, 1, 1, 2, 0);
        act.styleId = QStringLiteral("row");
        act.columnTracks = {PageTrackSize::starWeight(), trackPx(144)};
        PageCell shown = cell(id, value, 0, 0, {}, QColor(), 1, QStringLiteral("value"), {}, icon);
        shown.textStyle = QStringLiteral("row");
        shown.styleId = QStringLiteral("joinLeft");
        if (valueBg.isValid()) {
            shown.style.background = valueBg;
            shown.style.foreground = ThemeColors::contrastOn(valueBg);
        }
        act.cells.push_back(std::move(shown));
        act.cells.push_back(editIcon(id + QStringLiteral("_edit"), command));
        line.subGrids.push_back(std::move(act));
        host.subGrids.push_back(std::move(line));
    };
    auto stepRow = [&](PageGrid& host, int row, const QString& id, const QString& name,
                       const QString& value, bool decOn, bool incOn, const QString& dec,
                       const QString& inc) {
        PageGrid line = makeNested(id + QStringLiteral("_row"), row, 0, 1, 2, 6);
        line.styleId = QStringLiteral("group");
        line.marginPx = 8;
        line.cells.push_back(rowLabel(id + QStringLiteral("_caption"), name));
        PageGrid act = makeNested(id + QStringLiteral("_act"), 0, 1, 1, 3, 0);
        act.styleId = QStringLiteral("row");
        act.columnTracks = {trackPx(72), PageTrackSize::starWeight(), trackPx(72)};
        auto end = [](const QString& endId, const QString& label, int col, bool on,
                      const QString& command, const char* style) {
            PageCell button = cell(endId, label, 0, col, on ? command : QString(), QColor(), 1,
                                   on ? QString() : QStringLiteral("label"));
            button.textStyle = QStringLiteral("row");
            button.styleId = QLatin1String(style);
            return button;
        };
        act.cells.push_back(end(id + QStringLiteral("_dec"), QStringLiteral("−"), 0, decOn, dec,
                                "joinLeft"));
        PageCell shown = cell(id, value, 0, 1, {}, QColor(), 1, QStringLiteral("value"));
        shown.styleId = QStringLiteral("join");
        act.cells.push_back(std::move(shown));
        act.cells.push_back(end(id + QStringLiteral("_inc"), QStringLiteral("+"), 2, incOn, inc,
                                "joinRight"));
        line.subGrids.push_back(std::move(act));
        host.subGrids.push_back(std::move(line));
    };

    PageCell heading = cell(QStringLiteral("title"),
                            topic ? QStringLiteral("Edit Topic") : QStringLiteral("Edit Phrase"),
                            0, 0, {}, QColor(), 1, QStringLiteral("label"));
    heading.textStyle = QStringLiteral("title");
    heading.styleId = QStringLiteral("plain");
    grid.cells.push_back(std::move(heading));

    const SoundboardButton* button = topic ? nullptr : m_board.findButton(m_editId);
    const SoundboardTopic* editedTopic = topic ? m_board.findTopic(m_editId) : nullptr;

    PageGrid body = makeNested(QStringLiteral("editor"), 1, 0, 1, 2, 12);
    body.columnTracks = {PageTrackSize::starWeight(), PageTrackSize::starWeight()};
    PageGrid left = makeNested(QStringLiteral("editor_left"), 0, 0, 4, 1, kFormFieldRowGap);
    left.styleId = QStringLiteral("plain");
    PageGrid right = makeNested(QStringLiteral("editor_right"), 0, 1, 4, 1, kFormFieldRowGap);
    right.styleId = QStringLiteral("plain");

    const QString shownName = m_formName.isEmpty() ? QStringLiteral("None") : m_formName;
    fieldRow(left, 0, QStringLiteral("form_name"), QStringLiteral("Name"), shownName, {}, QColor(),
             QStringLiteral("compose.formField.name"));
    fieldRow(left, 1, QStringLiteral("form_icon"), QStringLiteral("Symbol"),
             m_editIcon.isEmpty() ? QStringLiteral("None") : QString(), m_editIcon, QColor(),
             QStringLiteral("compose.editShowIcons"));
    const auto colorFill = [&](const QString& stored) {
        return stored.isEmpty() ? QColor() : parseColor(stored, key);
    };
    fieldRow(left, 2, QStringLiteral("form_bg"), QStringLiteral("Background"),
             m_editColor.isEmpty() ? QStringLiteral("None") : m_editColor, {}, colorFill(m_editColor),
             QStringLiteral("compose.editShowColors"));
    fieldRow(left, 3, QStringLiteral("form_border"), QStringLiteral("Border"),
             m_editBorder.isEmpty() ? QStringLiteral("None") : m_editBorder, {},
             colorFill(m_editBorder), QStringLiteral("compose.editShowBorder"));

    if (topic) {
        const int minCols = formOccupiedSpan(false);
        const int minRows = formOccupiedSpan(true);
        const int cols = qBound(minCols, m_formCols, SoundboardStore::kMaxCols);
        const int boardRows = qBound(minRows, m_formRows, SoundboardStore::kMaxRows);
        stepRow(right, 0, QStringLiteral("rows"), QStringLiteral("Rows"), QString::number(boardRows),
                boardRows > minRows, boardRows < SoundboardStore::kMaxRows,
                QStringLiteral("compose.formRows.-1"), QStringLiteral("compose.formRows.1"));
        stepRow(right, 1, QStringLiteral("cols"), QStringLiteral("Columns"), QString::number(cols),
                cols > minCols, cols < SoundboardStore::kMaxCols, QStringLiteral("compose.formCols.-1"),
                QStringLiteral("compose.formCols.1"));
    } else {
        fieldRow(right, 0, QStringLiteral("form_phrase"), QStringLiteral("Phrase"),
                 m_formPhrase.isEmpty() ? QStringLiteral("None") : m_formPhrase, {}, QColor(),
                 QStringLiteral("compose.formField.phrase"));
        const bool hasAudio = !m_formAudioName.isEmpty() || !m_formAudioPath.isEmpty();
        fieldRow(right, 1, QStringLiteral("form_audio"), QStringLiteral("Audio"),
                 hasAudio ? m_formAudioName : QStringLiteral("None"), {}, QColor(),
                 QStringLiteral("compose.browseAudio"),
                 QStringLiteral("Plays instead of the phrase"));
    }

    const int count = formItemCount();
    const int index = count <= 0 ? 0 : qBound(0, m_formIndex, count - 1);
    stepRow(right, 2, QStringLiteral("idx"), QStringLiteral("Index"),
            QStringLiteral("%1 of %2").arg(count <= 0 ? 0 : index + 1).arg(count),
            count > 0 && index > 0, count > 0 && index < count - 1,
            QStringLiteral("compose.formIndex.-1"), QStringLiteral("compose.formIndex.1"));

    PageGrid previewRow = makeNested(QStringLiteral("preview_row"), 3, 0, 1, 2, 6);
    previewRow.styleId = QStringLiteral("group");
    previewRow.marginPx = 8;
    previewRow.cells.push_back(rowLabel(QStringLiteral("preview_caption"), QStringLiteral("Preview")));
    QString faceLabel = m_formName.trimmed();
    if (faceLabel.isEmpty()) {
        faceLabel = editedTopic ? editedTopic->name : (button ? button->label : QString());
    }
    PageCell tile = cell(QStringLiteral("preview_face"), faceLabel, 0, 1, {}, key, 1,
                         QStringLiteral("face"), {}, m_editIcon);
    tile.textStyle = QStringLiteral("row");
    paintItemFace(tile, m_editColor, m_editBorder, key);
    tile.style.radius = PageBox::all(10);
    previewRow.cells.push_back(std::move(tile));
    right.subGrids.push_back(std::move(previewRow));

    body.subGrids.push_back(std::move(left));
    body.subGrids.push_back(std::move(right));
    grid.subGrids.push_back(std::move(body));

    PageGrid foot = makeNested(QStringLiteral("footer"), 2, 0, 1, 4, 8);
    foot.columnTracks = {PageTrackSize::starWeight(1.0), PageTrackSize::starWeight(1.2),
                         PageTrackSize::starWeight(1.3), PageTrackSize::starWeight(1.0)};
    PageCell remove = cell(QStringLiteral("form_delete"), QStringLiteral("Delete"), 0, 0,
                           QStringLiteral("compose.deleteName"), QColor());
    paintDanger(remove, danger);
    remove.style.radius = PageBox::all(10);
    foot.cells.push_back(std::move(remove));
    PageCell exp = cell(QStringLiteral("form_export"), QStringLiteral("Export"), 0, 1,
                        topic ? QStringLiteral("compose.exportTopic")
                              : QStringLiteral("compose.exportPhrase"),
                        key);
    paintSurface(exp, key);
    exp.style.radius = PageBox::all(10);
    foot.cells.push_back(std::move(exp));
    PageCell save = cell(QStringLiteral("form_save"), QStringLiteral("Save"), 0, 2,
                         QStringLiteral("compose.saveName"), accent);
    paintSave(save, accent);
    save.style.radius = PageBox::all(10);
    foot.cells.push_back(std::move(save));
    PageCell cancel = cell(QStringLiteral("form_cancel"), QStringLiteral("Cancel"), 0, 3,
                           QStringLiteral("compose.cancelName"), key);
    paintSurface(cancel, key);
    cancel.style.radius = PageBox::all(10);
    foot.cells.push_back(std::move(cancel));
    grid.subGrids.push_back(std::move(foot));
    return doc;
}

} // namespace gazer

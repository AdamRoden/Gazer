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

int occupiedSpan(const SoundboardTopic* topic, bool rows)
{
    int n = 1;
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

} // namespace

QString ComposeUi::formNameText() const
{
    if (m_formField == FormField::Phrase) {
        return m_formName.trimmed();
    }
    return m_buffer.text().trimmed();
}

QString ComposeUi::formPhraseText() const
{
    if (m_formField == FormField::Name) {
        return m_formPhrase.trimmed();
    }
    return m_buffer.text().trimmed();
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

void ComposeUi::focusFormField(const QString& which)
{
    if (!nameEditing()) {
        return;
    }
    const bool phrase = which == QLatin1String("phrase");
    if (phrase && m_nameEdit != NameEditKind::Pin) {
        return;
    }
    if (!phrase && which != QLatin1String("name")) {
        return;
    }
    const FormField next = phrase ? FormField::Phrase : FormField::Name;
    if (next != m_formField) {
        if (m_formField == FormField::Name) {
            m_formName = m_buffer.text();
        } else {
            m_formPhrase = m_buffer.text();
            m_formPhraseTouched = true;
        }
        m_formField = next;
        if (next == FormField::Phrase) {
            m_formPhraseTouched = true;
            m_buffer.load(m_formPhrase);
        } else {
            m_buffer.load(m_formName);
        }
        if (m_typing) {
            m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
        }
    }
    rebuildBoard();
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
}

void ComposeUi::nudgeFormCols(int delta)
{
    if (m_nameEdit != NameEditKind::Topic || delta == 0) {
        return;
    }
    const int minCols = occupiedSpan(m_board.findTopic(m_editId), false);
    m_formCols = qBound(minCols, m_formCols + delta, SoundboardStore::kMaxCols);
    rebuildForm();
}

void ComposeUi::nudgeFormRows(int delta)
{
    if (m_nameEdit != NameEditKind::Topic || delta == 0) {
        return;
    }
    const int minRows = occupiedSpan(m_board.findTopic(m_editId), true);
    m_formRows = qBound(minRows, m_formRows + delta, SoundboardStore::kMaxRows);
    rebuildForm();
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
    PageChrome plain;
    plain.background.token = QStringLiteral("transparent");
    plain.thickness = PageBox::all(0);
    plain.radius = PageBox::all(0);
    doc.styles.insert(QStringLiteral("plain"), plain);
    const ThemeColors theme = m_settings.resolvedTheme();
    initTopOverlay(doc, 1, 3, {1.0, 8.0, 1.2}, 8, 12, theme, aboveKeyboardHeight(m_composeAuthored),
                   speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = {trackPx(56), PageTrackSize::starWeight(), trackPx(72)};
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor field = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QColor danger = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor rim = ThemeColors::mix(field, ThemeColors::contrastOn(field), 0.28);
    const QVector<PageTrackSize> labelTracks = {trackPx(148), PageTrackSize::starWeight()};

    auto caption = [&](const QString& id, const QString& label, int col = 0) {
        PageCell text = cell(id, label, 0, col, {}, QColor(), 1, QStringLiteral("label"));
        text.textStyle = QStringLiteral("caption");
        text.styleId = QStringLiteral("plain");
        text.style.background = QColor(0, 0, 0, 0);
        text.style.thickness = PageBox::all(0);
        text.style.radius = PageBox::all(0);
        return text;
    };
    auto fieldChrome = [&](PageCell& box, bool focused) {
        box.style.background = field;
        box.style.foreground = ThemeColors::contrastOn(field);
        box.style.radius = PageBox::all(12);
        box.style.borderColor = focused ? accent : rim;
        box.style.thickness = PageBox::all(focused ? 2 : 1);
    };
    auto stepEnd = [&](const QString& id, const QString& label, int col, bool on,
                       const QString& command) {
        PageCell button = cell(id, label, 0, col, on ? command : QString(), key, 1,
                               on ? QString() : QStringLiteral("label"));
        button.style.radius = PageBox::all(12);
        return button;
    };

    PageCell heading = cell(QStringLiteral("title"),
                            topic ? QStringLiteral("Edit Topic") : QStringLiteral("Edit Phrase"),
                            0, 0, {}, QColor(), 1, QStringLiteral("label"));
    heading.textStyle = QStringLiteral("title");
    heading.styleId = QStringLiteral("plain");
    heading.style.background = QColor(0, 0, 0, 0);
    heading.style.thickness = PageBox::all(0);
    heading.style.radius = PageBox::all(0);
    grid.cells.push_back(std::move(heading));

    const SoundboardButton* button = nullptr;
    const SoundboardTopic* editedTopic = nullptr;
    if (topic) {
        editedTopic = m_board.findTopic(m_editId);
    } else {
        button = m_board.findButton(m_editId);
    }

    PageGrid body = makeNested(QStringLiteral("editor"), 1, 0, topic ? 2 : 1, 2, 12);
    body.columnTracks = {PageTrackSize::starWeight(), PageTrackSize::starWeight()};
    if (topic) {
        body.rowTracks = {PageTrackSize::starWeight(4), PageTrackSize::starWeight(2)};
    }

    const QVector<PageTrackSize> phraseRows = starTracks({1, 1, 1, 1});
    PageGrid left = makeNested(QStringLiteral("editor_left"), 0, 0, topic ? 3 : 4, 1, 8);
    left.rowTracks = topic ? starTracks({1, 1, 1}) : phraseRows;
    PageGrid right = makeNested(QStringLiteral("editor_right"), 0, 1, 4, 1, 8);
    right.rowTracks = phraseRows;

    const bool nameFocus = m_formField == FormField::Name;
    const QString nameText = nameFocus ? m_buffer.text() : m_formName;
    PageGrid nameRow = makeNested(QStringLiteral("name_row"), 0, 0, 1, 2, 8);
    nameRow.columnTracks = labelTracks;
    nameRow.cells.push_back(caption(QStringLiteral("name_caption"), QStringLiteral("Name")));
    PageCell name = cell(QStringLiteral("form_name"),
                         nameText.isEmpty() ? QStringLiteral("Name") : nameText, 0, 1,
                         QStringLiteral("compose.formField.name"), field);
    fieldChrome(name, nameFocus);
    if (nameFocus) {
        name.caretIndex = nameText.isEmpty() ? 0 : qBound(0, m_buffer.caret(), name.label.size());
    }
    nameRow.cells.push_back(std::move(name));
    left.subGrids.push_back(std::move(nameRow));

    auto colorRow = [&](PageGrid& host, int row, const QString& id, const QString& label,
                        const QString& stored, const QString& command) {
        PageGrid line = makeNested(id + QStringLiteral("_row"), row, 0, 1, 3, 8);
        line.columnTracks = {trackPx(148), trackPx(72), PageTrackSize::starWeight()};
        line.cells.push_back(caption(id + QStringLiteral("_caption"), label));
        const QColor ink = stored.isEmpty() ? field : parseColor(stored, key);
        PageCell swatch = cell(id, {}, 0, 1, command, ink);
        swatch.style.radius = PageBox::all(12);
        line.cells.push_back(std::move(swatch));
        PageCell hex = cell(id + QStringLiteral("_hex"),
                            stored.isEmpty() ? QStringLiteral("None") : stored, 0, 2, command, field);
        fieldChrome(hex, false);
        line.cells.push_back(std::move(hex));
        host.subGrids.push_back(std::move(line));
    };
    colorRow(left, 1, QStringLiteral("form_bg"), QStringLiteral("Background"), m_editColor,
             QStringLiteral("compose.editShowColors"));
    colorRow(right, 1, QStringLiteral("form_border"), QStringLiteral("Border"), m_editBorder,
             QStringLiteral("compose.editShowBorder"));

    PageGrid symbolRow = makeNested(QStringLiteral("symbol_row"), 0, 0, 1, 3, 8);
    symbolRow.columnTracks = {trackPx(148), trackPx(72), PageTrackSize::starWeight()};
    symbolRow.cells.push_back(caption(QStringLiteral("symbol_caption"), QStringLiteral("Symbol")));
    PageCell search = cell(QStringLiteral("form_icon_search"), {}, 0, 1,
                           QStringLiteral("compose.editShowIcons"), key, 1, {}, {},
                           QStringLiteral("MagnifyTool"));
    search.style.radius = PageBox::all(12);
    symbolRow.cells.push_back(std::move(search));
    PageCell symbol = cell(QStringLiteral("form_icon"),
                           m_editIcon.isEmpty() ? QStringLiteral("None") : QString(), 0, 2,
                           QStringLiteral("compose.editShowIcons"), field, 1, {}, {}, m_editIcon);
    fieldChrome(symbol, false);
    symbolRow.cells.push_back(std::move(symbol));
    right.subGrids.push_back(std::move(symbolRow));

    auto stepper = [&](PageGrid& host, int row, const QString& id, const QString& label, int value,
                       bool decOn, bool incOn, const QString& decCmd, const QString& incCmd) {
        PageGrid line = makeNested(id + QStringLiteral("_row"), row, 0, 1, 4, 8);
        line.columnTracks = {trackPx(148), trackPx(64), PageTrackSize::starWeight(), trackPx(64)};
        line.cells.push_back(caption(id + QStringLiteral("_label"), label));
        line.cells.push_back(stepEnd(id + QStringLiteral("_dec"), QStringLiteral("−"), 1, decOn, decCmd));
        PageCell shown = cell(id + QStringLiteral("_val"), QString::number(value), 0, 2, {}, field, 1,
                              QStringLiteral("value"));
        fieldChrome(shown, false);
        line.cells.push_back(std::move(shown));
        line.cells.push_back(stepEnd(id + QStringLiteral("_inc"), QStringLiteral("+"), 3, incOn, incCmd));
        host.subGrids.push_back(std::move(line));
    };

    if (topic) {
        const int minCols = occupiedSpan(editedTopic, false);
        const int minRows = occupiedSpan(editedTopic, true);
        const int cols = qBound(minCols, m_formCols, SoundboardStore::kMaxCols);
        const int boardRows = qBound(minRows, m_formRows, SoundboardStore::kMaxRows);
        stepper(right, 2, QStringLiteral("rows"), QStringLiteral("Rows"), boardRows,
                boardRows > minRows, boardRows < SoundboardStore::kMaxRows,
                QStringLiteral("compose.formRows.-1"), QStringLiteral("compose.formRows.1"));
        stepper(right, 3, QStringLiteral("cols"), QStringLiteral("Columns"), cols, cols > minCols,
                cols < SoundboardStore::kMaxCols, QStringLiteral("compose.formCols.-1"),
                QStringLiteral("compose.formCols.1"));
    } else {
        const bool phraseFocus = m_formField == FormField::Phrase;
        const QString phraseText = phraseFocus ? m_buffer.text() : m_formPhrase;
        QString placeholder = QStringLiteral("Phrase");
        if (phraseText.isEmpty() && button) {
            if (!button->sourceText.trimmed().isEmpty()) {
                placeholder = button->sourceText.trimmed();
            } else if (!button->label.isEmpty()) {
                placeholder = button->label;
            }
        }
        PageGrid phraseRow = makeNested(QStringLiteral("phrase_row"), 2, 0, 1, 2, 8);
        phraseRow.columnTracks = labelTracks;
        phraseRow.cells.push_back(caption(QStringLiteral("phrase_caption"), QStringLiteral("Phrase")));
        PageCell phrase = cell(QStringLiteral("form_phrase"),
                               phraseText.isEmpty() ? placeholder : phraseText, 0, 1,
                               QStringLiteral("compose.formField.phrase"), field);
        fieldChrome(phrase, phraseFocus);
        if (phraseFocus) {
            phrase.caretIndex =
                phraseText.isEmpty() ? 0 : qBound(0, m_buffer.caret(), phrase.label.size());
        }
        phraseRow.cells.push_back(std::move(phrase));
        left.subGrids.push_back(std::move(phraseRow));

        QString audioLabel = QStringLiteral("None");
        if (!m_formAudioName.isEmpty()) {
            audioLabel = m_formAudioName;
        }
        PageGrid audio = makeNested(QStringLiteral("audio_row"), 2, 0, 1, 3, 8);
        audio.columnTracks = {trackPx(148), PageTrackSize::starWeight(), trackPx(148)};
        audio.cells.push_back(caption(QStringLiteral("audio_caption"), QStringLiteral("Audio")));
        PageCell file = cell(QStringLiteral("form_audio"), audioLabel, 0, 1,
                             QStringLiteral("compose.browseAudio"), field);
        fieldChrome(file, false);
        file.caption = m_formAudioPath.isEmpty() && (button == nullptr || button->clipId.isEmpty())
                           ? QString()
                           : QStringLiteral("Plays instead of the phrase");
        audio.cells.push_back(std::move(file));
        PageCell browse = cell(QStringLiteral("form_audio_browse"), QStringLiteral("Browse"), 0, 2,
                               QStringLiteral("compose.browseAudio"), key, 1, {}, {},
                               QStringLiteral("folder"));
        browse.style.radius = PageBox::all(12);
        audio.cells.push_back(std::move(browse));
        right.subGrids.push_back(std::move(audio));
    }

    const int count = formItemCount();
    const int index = count <= 0 ? 0 : qBound(0, m_formIndex, count - 1);
    const bool canDec = count > 0 && index > 0;
    const bool canInc = count > 0 && index < count - 1;
    PageGrid indexRow = makeNested(QStringLiteral("index_row"), topic ? 2 : 3, 0, 1, 4, 8);
    indexRow.columnTracks = {trackPx(148), trackPx(64), PageTrackSize::starWeight(), trackPx(64)};
    PageCell indexLabel = caption(QStringLiteral("idx_label"), QStringLiteral("Index"));
    indexLabel.caption = QStringLiteral("%1 of %2").arg(count <= 0 ? 0 : index + 1).arg(count);
    indexRow.cells.push_back(std::move(indexLabel));
    indexRow.cells.push_back(stepEnd(QStringLiteral("idx_dec"), QStringLiteral("−"), 1, canDec,
                                     QStringLiteral("compose.formIndex.-1")));
    PageCell idx = cell(QStringLiteral("idx_val"), QString::number(count <= 0 ? 0 : index + 1), 0, 2,
                        {}, field, 1, QStringLiteral("value"));
    fieldChrome(idx, false);
    indexRow.cells.push_back(std::move(idx));
    indexRow.cells.push_back(stepEnd(QStringLiteral("idx_inc"), QStringLiteral("+"), 3, canInc,
                                     QStringLiteral("compose.formIndex.1")));
    left.subGrids.push_back(std::move(indexRow));

    auto boardBackdrop = [](PageGrid& board) {
        board.style.background.token = QStringLiteral("bg95");
        board.style.radius = PageBox::all(16);
        board.style.thickness = PageBox::all(0);
        board.marginPx = 8;
    };
    if (topic) {
        const int cols = qBound(occupiedSpan(editedTopic, false), m_formCols, SoundboardStore::kMaxCols);
        const int boardRows = qBound(occupiedSpan(editedTopic, true), m_formRows, SoundboardStore::kMaxRows);
        PageGrid preview = makeNested(QStringLiteral("preview"), 1, 0, boardRows, cols, 8);
        preview.colSpan = 2;
        boardBackdrop(preview);
        for (int r = 0; r < boardRows; ++r) {
            preview.rowTracks.push_back(PageTrackSize::starWeight());
        }
        for (int c = 0; c < cols; ++c) {
            preview.columnTracks.push_back(PageTrackSize::starWeight());
        }
        if (!editedTopic || editedTopic->buttons.isEmpty()) {
            PageCell empty = caption(QStringLiteral("preview_empty"), QStringLiteral("No phrases"));
            empty.colSpan = cols;
            empty.rowSpan = boardRows;
            preview.cells.push_back(std::move(empty));
        } else {
            for (int i = 0; i < editedTopic->buttons.size(); ++i) {
                const SoundboardButton& source = editedTopic->buttons.at(i);
                if (source.row < 0 || source.col < 0 || source.row >= boardRows || source.col >= cols) {
                    continue;
                }
                PageCell tile;
                tile.id = QStringLiteral("preview_%1").arg(i);
                tile.role = QStringLiteral("face");
                tile.row = source.row;
                tile.col = source.col;
                tile.label = source.label;
                tile.icon = source.icon;
                paintItemFace(tile, source.color, source.borderColor, key);
                preview.cells.push_back(std::move(tile));
            }
        }
        body.subGrids.push_back(std::move(preview));
    } else {
        PageGrid previewRow = makeNested(QStringLiteral("preview_row"), 3, 0, 1, 2, 8);
        previewRow.columnTracks = labelTracks;
        previewRow.cells.push_back(caption(QStringLiteral("preview_caption"), QStringLiteral("Preview")));
        PageGrid preview = makeNested(QStringLiteral("preview"), 0, 1, 1, 3, 8);
        preview.columnTracks = {PageTrackSize::starWeight(), PageTrackSize::starWeight(1.4),
                                PageTrackSize::starWeight()};
        boardBackdrop(preview);
        if (button) {
            PageCell tile = cell(QStringLiteral("preview_phrase"),
                                 formNameText().isEmpty() ? button->label : formNameText(), 0, 1, {},
                                 key, 1, QStringLiteral("face"), {}, m_editIcon);
            paintItemFace(tile, m_editColor, m_editBorder, key);
            preview.cells.push_back(std::move(tile));
        }
        previewRow.subGrids.push_back(std::move(preview));
        right.subGrids.push_back(std::move(previewRow));
    }

    body.subGrids.push_back(std::move(left));
    body.subGrids.push_back(std::move(right));
    grid.subGrids.push_back(std::move(body));

    PageGrid foot = makeNested(QStringLiteral("footer"), 2, 0, 1, 3, 8);
    foot.columnTracks = {PageTrackSize::starWeight(1.0), PageTrackSize::starWeight(1.4),
                         PageTrackSize::starWeight(1.0)};
    PageCell remove = cell(QStringLiteral("form_delete"), QStringLiteral("Delete"), 0, 0,
                           QStringLiteral("compose.deleteName"), QColor());
    paintDanger(remove, danger);
    foot.cells.push_back(std::move(remove));
    PageCell save = cell(QStringLiteral("form_save"), QStringLiteral("Save"), 0, 1,
                         QStringLiteral("compose.saveName"), accent);
    paintSave(save, accent);
    foot.cells.push_back(std::move(save));
    PageCell cancel = cell(QStringLiteral("form_cancel"), QStringLiteral("Cancel"), 0, 2,
                           QStringLiteral("compose.cancelName"), key);
    paintSurface(cancel, key);
    foot.cells.push_back(std::move(cancel));
    grid.subGrids.push_back(std::move(foot));
    return doc;
}

} // namespace gazer

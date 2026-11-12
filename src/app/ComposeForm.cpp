#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/SoundboardStore.h"
#include "layout/PageSession.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QtGlobal>
#include <algorithm>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using SettingsPageBuild::makeNested;
using compose_detail::aboveKeyboardHeight;
using compose_detail::paintDanger;
using compose_detail::paintOutline;
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

int rowMajorSlot(const SoundboardTopic& topic, const QString& buttonId)
{
    QVector<int> order;
    order.reserve(topic.buttons.size());
    for (int i = 0; i < topic.buttons.size(); ++i) {
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const SoundboardButton& ba = topic.buttons.at(a);
        const SoundboardButton& bb = topic.buttons.at(b);
        if (ba.row != bb.row) {
            return ba.row < bb.row;
        }
        if (ba.col != bb.col) {
            return ba.col < bb.col;
        }
        return a < b;
    });
    for (int slot = 0; slot < order.size(); ++slot) {
        if (topic.buttons.at(order.at(slot)).id == buttonId) {
            return slot;
        }
    }
    return -1;
}

QVector<int> rowMajorOrder(const SoundboardTopic& topic)
{
    QVector<int> order;
    order.reserve(topic.buttons.size());
    for (int i = 0; i < topic.buttons.size(); ++i) {
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const SoundboardButton& ba = topic.buttons.at(a);
        const SoundboardButton& bb = topic.buttons.at(b);
        if (ba.row != bb.row) {
            return ba.row < bb.row;
        }
        if (ba.col != bb.col) {
            return ba.col < bb.col;
        }
        return a < b;
    });
    return order;
}

void paintSpoken(PageCell& cell, const SoundboardStore::PlayPlan& plan, const QColor& ink,
                 const QColor& surface)
{
    if (plan.kind == SoundboardStore::PlayPlan::Kind::Utterance
        || plan.kind == SoundboardStore::PlayPlan::Kind::Source) {
        paintOutline(cell, ink);
        return;
    }
    const QColor bg = ink.isValid() ? ink : surface;
    cell.style.background = bg;
    cell.style.foreground = ThemeColors::contrastOn(bg);
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
            return topic->buttons.size();
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
            const int slot = rowMajorSlot(topic, m_editId);
            if (slot >= 0) {
                return slot;
            }
        }
    }
    return -1;
}

void ComposeUi::applyFormDrafts()
{
    // Spoken text only. Copying the label or source into utteranceText would
    // hide a clip, because a phrase wins over the clip when it plays.
    QString err;
    if (m_nameEdit == NameEditKind::Pin && m_formPhraseTouched) {
        (void)m_board.setButtonUtterance(m_editId, formPhraseText(), &err);
    }
    if (m_nameEdit == NameEditKind::Topic) {
        (void)m_board.setTopicGridCols(m_editId, m_formCols, &err);
    }
    const int live = formLiveIndex();
    const int count = formItemCount();
    if (live < 0 || count <= 0) {
        return;
    }
    const int want = qBound(0, m_formIndex, count - 1);
    const int delta = want - live;
    if (delta == 0) {
        return;
    }
    if (m_nameEdit == NameEditKind::Topic) {
        (void)m_board.moveTopic(m_editId, delta, &err);
    } else if (m_nameEdit == NameEditKind::Pin) {
        (void)m_board.moveButton(m_editId, delta, &err);
    }
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
    m_formCols = qBound(1, m_formCols + delta, SoundboardStore::kMaxCols);
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
    const int rows = topic ? 9 : 11;
    PageDocument doc;
    doc.id = QString(kFormLiveId);
    doc.name = topic ? QStringLiteral("Edit Topic") : QStringLiteral("Edit Phrase");
    const ThemeColors theme = m_settings.resolvedTheme();
    QVector<double> weights(rows, 1.0);
    initTopOverlay(doc, 1, rows, weights, 8, 16, theme, aboveKeyboardHeight(m_composeAuthored),
                   speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor field = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QColor danger = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor rim = ThemeColors::mix(field, ThemeColors::contrastOn(field), 0.28);

    int row = 0;
    grid.rowTracks.clear();
    auto take = [&](int px) {
        grid.rowTracks.push_back(px <= 0 ? PageTrackSize::starWeight() : trackPx(px));
        return row++;
    };
    auto section = [&](const QString& id, const QString& label, int col = 0, int span = 1) {
        PageCell caption = cell(id, label, 0, col, {}, QColor(), span, QStringLiteral("label"));
        caption.textStyle = QStringLiteral("caption");
        return caption;
    };
    auto fieldChrome = [&](PageCell& box, bool focused) {
        box.style.background = field;
        box.style.foreground = ThemeColors::contrastOn(field);
        box.style.radius = PageBox::all(12);
        box.style.borderColor = focused ? accent : rim;
        box.style.thickness = PageBox::all(focused ? 2 : 1);
    };

    const int titleRow = take(64);
    PageCell heading = cell(QStringLiteral("title"),
                            topic ? QStringLiteral("Edit Topic") : QStringLiteral("Edit Phrase"),
                            titleRow, 0, {}, QColor(), 1, QStringLiteral("label"));
    heading.textStyle = QStringLiteral("title");
    grid.cells.push_back(std::move(heading));

    const int nameLabelRow = take(28);
    PageGrid nameLabel = makeNested(QStringLiteral("name_label"), nameLabelRow, 0, 1, 1, 0);
    nameLabel.cells.push_back(section(QStringLiteral("name_caption"), QStringLiteral("Name")));
    grid.subGrids.push_back(std::move(nameLabel));

    const bool nameFocus = m_formField == FormField::Name;
    const QString nameText = nameFocus ? m_buffer.text() : m_formName;
    const int nameRow = take(72);
    PageCell name = cell(QStringLiteral("form_name"),
                         nameText.isEmpty() ? QStringLiteral("Name") : nameText, nameRow, 0,
                         QStringLiteral("compose.formField.name"), field);
    fieldChrome(name, nameFocus);
    if (nameFocus) {
        name.caretIndex = nameText.isEmpty() ? 0 : qBound(0, m_buffer.caret(), name.label.size());
    }
    grid.cells.push_back(std::move(name));

    const QVector<PageTrackSize> colorTracks = {trackPx(72), PageTrackSize::starWeight(),
                                                PageTrackSize::starWeight(), trackPx(72)};
    const int colorLabelRow = take(28);
    PageGrid colorLabels = makeNested(QStringLiteral("color_labels"), colorLabelRow, 0, 1, 4, 8);
    colorLabels.columnTracks = colorTracks;
    colorLabels.cells.push_back(
        section(QStringLiteral("color_caption"), QStringLiteral("Color"), 0, 2));
    colorLabels.cells.push_back(
        section(QStringLiteral("symbol_caption"), QStringLiteral("Symbol"), 2, 2));
    grid.subGrids.push_back(std::move(colorLabels));

    const QColor ink = parseColor(m_editColor, key);
    const int colorRow = take(72);
    PageGrid color = makeNested(QStringLiteral("color_row"), colorRow, 0, 1, 4, 8);
    color.columnTracks = colorTracks;
    PageCell swatch = cell(QStringLiteral("form_color"), {}, 0, 0,
                           QStringLiteral("compose.editShowColors"), ink);
    swatch.style.radius = PageBox::all(12);
    color.cells.push_back(std::move(swatch));
    PageCell hex = cell(QStringLiteral("form_hex"),
                        m_editColor.isEmpty() ? QStringLiteral("None") : m_editColor, 0, 1, {},
                        field, 1, QStringLiteral("label"));
    hex.style.radius = PageBox::all(12);
    color.cells.push_back(std::move(hex));
    PageCell symbol = cell(QStringLiteral("form_icon"),
                           m_editIcon.isEmpty() ? QStringLiteral("Symbol") : m_editIcon, 0, 2, {},
                           field, 1, QStringLiteral("label"));
    symbol.style.radius = PageBox::all(12);
    color.cells.push_back(std::move(symbol));
    PageCell search = cell(QStringLiteral("form_icon_search"), {}, 0, 3,
                           QStringLiteral("compose.editShowIcons"), key, 1, {}, {},
                           QStringLiteral("search"));
    search.style.radius = PageBox::all(12);
    color.cells.push_back(std::move(search));
    grid.subGrids.push_back(std::move(color));

    const SoundboardButton* button = nullptr;
    const SoundboardTopic* editedTopic = nullptr;
    if (topic) {
        editedTopic = m_board.findTopic(m_editId);
    } else {
        button = m_board.findButton(m_editId);
    }

    if (!topic) {
        const int phraseLabelRow = take(28);
        PageGrid phraseLabel = makeNested(QStringLiteral("phrase_label"), phraseLabelRow, 0, 1, 1, 0);
        phraseLabel.cells.push_back(
            section(QStringLiteral("phrase_caption"), QStringLiteral("Phrase")));
        grid.subGrids.push_back(std::move(phraseLabel));

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
        const int phraseRow = take(72);
        PageCell phrase = cell(QStringLiteral("form_phrase"),
                               phraseText.isEmpty() ? placeholder : phraseText, phraseRow, 0,
                               QStringLiteral("compose.formField.phrase"), field);
        fieldChrome(phrase, phraseFocus);
        if (phraseFocus) {
            phrase.caretIndex =
                phraseText.isEmpty() ? 0 : qBound(0, m_buffer.caret(), phrase.label.size());
        }
        grid.cells.push_back(std::move(phrase));
    }

    const int count = formItemCount();
    const int index = count <= 0 ? 0 : qBound(0, m_formIndex, count - 1);
    const int stepperRow = take(72);
    const int stepperCols = topic ? 9 : 5;
    PageGrid steps = makeNested(QStringLiteral("steppers"), stepperRow, 0, 1, stepperCols, 8);
    auto stepEnd = [&](const QString& id, const QString& label, int col, bool on,
                       const QString& command) {
        PageCell button = cell(id, label, 0, col, on ? command : QString(), key, 1,
                               on ? QString() : QStringLiteral("label"));
        button.style.radius = PageBox::all(12);
        return button;
    };
    const bool canDec = count > 0 && index > 0;
    const bool canInc = count > 0 && index < count - 1;
    PageCell indexLabel = section(QStringLiteral("idx_label"), QStringLiteral("Index"));
    indexLabel.caption = QStringLiteral("%1 of %2").arg(count <= 0 ? 0 : index + 1).arg(count);
    if (topic) {
        steps.columnTracks = {trackPx(88), trackPx(64), trackPx(64), trackPx(64), trackPx(72),
                              trackPx(64), trackPx(64), trackPx(64), PageTrackSize::starWeight()};
        steps.cells.push_back(std::move(indexLabel));
        steps.cells.push_back(stepEnd(QStringLiteral("idx_dec"), QStringLiteral("−"), 1, canDec,
                                      QStringLiteral("compose.formIndex.-1")));
        PageCell idx = cell(QStringLiteral("idx_val"), QString::number(count <= 0 ? 0 : index + 1),
                            0, 2, {}, field, 1, QStringLiteral("label"));
        idx.style.radius = PageBox::all(12);
        steps.cells.push_back(std::move(idx));
        steps.cells.push_back(stepEnd(QStringLiteral("idx_inc"), QStringLiteral("+"), 3, canInc,
                                      QStringLiteral("compose.formIndex.1")));
        steps.cells.push_back(section(QStringLiteral("cols_label"), QStringLiteral("Cols"), 4));
        const int cols = qBound(1, m_formCols, SoundboardStore::kMaxCols);
        const bool colsDec = cols > 1;
        const bool colsInc = cols < SoundboardStore::kMaxCols;
        steps.cells.push_back(stepEnd(QStringLiteral("cols_dec"), QStringLiteral("−"), 5, colsDec,
                                      QStringLiteral("compose.formCols.-1")));
        PageCell colsCell = cell(QStringLiteral("cols_val"), QString::number(cols), 0, 6, {}, field,
                                 1, QStringLiteral("label"));
        colsCell.style.radius = PageBox::all(12);
        steps.cells.push_back(std::move(colsCell));
        steps.cells.push_back(stepEnd(QStringLiteral("cols_inc"), QStringLiteral("+"), 7, colsInc,
                                      QStringLiteral("compose.formCols.1")));
    } else {
        steps.columnTracks = {trackPx(88), PageTrackSize::starWeight(), trackPx(64), trackPx(64),
                              trackPx(64)};
        steps.cells.push_back(std::move(indexLabel));
        steps.cells.push_back(stepEnd(QStringLiteral("idx_dec"), QStringLiteral("−"), 2, canDec,
                                      QStringLiteral("compose.formIndex.-1")));
        PageCell idx = cell(QStringLiteral("idx_val"), QString::number(count <= 0 ? 0 : index + 1),
                            0, 3, {}, field, 1, QStringLiteral("label"));
        idx.style.radius = PageBox::all(12);
        steps.cells.push_back(std::move(idx));
        steps.cells.push_back(stepEnd(QStringLiteral("idx_inc"), QStringLiteral("+"), 4, canInc,
                                      QStringLiteral("compose.formIndex.1")));
    }
    grid.subGrids.push_back(std::move(steps));

    const int previewLabelRow = take(28);
    PageGrid previewLabel =
        makeNested(QStringLiteral("preview_label"), previewLabelRow, 0, 1, 1, 0);
    previewLabel.cells.push_back(section(QStringLiteral("preview_caption"),
                                        topic ? QStringLiteral("Sound buttons")
                                              : QStringLiteral("Preview")));
    grid.subGrids.push_back(std::move(previewLabel));

    const int previewRow = take(0);
    PageGrid preview = makeNested(QStringLiteral("preview"), previewRow, 0, 1, 1, 8);
    preview.style.radius = PageBox::all(16);
    preview.style.background = field;
    preview.style.borderColor = rim;
    preview.style.thickness = PageBox::all(1);
    if (topic) {
        const int cols = qBound(1, m_formCols, SoundboardStore::kMaxCols);
        if (!editedTopic || editedTopic->buttons.isEmpty()) {
            preview.cells.push_back(cell(QStringLiteral("preview_empty"),
                                        QStringLiteral("No phrases"), 0, 0, {}, QColor(), 1,
                                        QStringLiteral("label")));
        } else {
            const QVector<int> order = rowMajorOrder(*editedTopic);
            const int buttonRows = (order.size() + cols - 1) / cols;
            PageGrid mini =
                makeNested(QStringLiteral("preview_grid"), 0, 0, buttonRows + 1, cols, 8);
            for (int r = 0; r < buttonRows; ++r) {
                mini.rowTracks.push_back(trackPx(80));
            }
            mini.rowTracks.push_back(PageTrackSize::starWeight());
            for (int c = 0; c < cols; ++c) {
                mini.columnTracks.push_back(PageTrackSize::starWeight());
            }
            for (int i = 0; i < order.size(); ++i) {
                const SoundboardButton& source = editedTopic->buttons.at(order.at(i));
                PageCell tile;
                tile.id = QStringLiteral("preview_%1").arg(i);
                tile.role = QStringLiteral("label");
                tile.row = i / cols;
                tile.col = i % cols;
                tile.label = source.label;
                tile.icon = source.icon;
                tile.style.radius = PageBox::all(12);
                const QColor tileInk = parseColor(source.color, key);
                paintSpoken(tile, m_board.planPlay(source), tileInk, field);
                mini.cells.push_back(std::move(tile));
            }
            preview.subGrids.push_back(std::move(mini));
        }
    } else if (button) {
        SoundboardButton draft = *button;
        draft.icon = m_editIcon;
        if (!m_editColor.isEmpty()) {
            draft.color = m_editColor;
        }
        const QString draftedName = formNameText();
        if (!draftedName.isEmpty()) {
            draft.label = draftedName;
        }
        if (m_formPhraseTouched) {
            draft.utteranceText = formPhraseText();
        }
        PageGrid mini = makeNested(QStringLiteral("preview_grid"), 0, 0, 2, 1, 0);
        mini.rowTracks = {trackPx(80), PageTrackSize::starWeight()};
        PageCell tile = cell(QStringLiteral("preview_phrase"), draft.label, 0, 0, {}, key, 1,
                             QStringLiteral("label"), {}, draft.icon);
        tile.style.radius = PageBox::all(12);
        paintSpoken(tile, m_board.planPlay(draft), parseColor(draft.color, key), field);
        mini.cells.push_back(std::move(tile));
        preview.subGrids.push_back(std::move(mini));
    }
    grid.subGrids.push_back(std::move(preview));

    const int footerRow = take(72);
    PageGrid foot = makeNested(QStringLiteral("footer"), footerRow, 0, 1, 3, 8);
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

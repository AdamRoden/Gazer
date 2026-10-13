#include "app/SettingsUi.h"

#include "app/AppSettings.h"
#include "assist/AudioOutput.h"
#include "assist/VoiceCatalog.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"

namespace gazer {

namespace {

QString outputCaption(const AppSettings& settings)
{
    if (settings.speechOutputId.isEmpty()) {
        const QString name = AudioOutput::defaultPlaybackName();
        if (name.isEmpty()) {
            return QStringLiteral("Follows the Windows default.");
        }
        return QStringLiteral("Follows Windows (%1).").arg(name);
    }
    const QString live = AudioOutput::playbackName(settings.speechOutputId);
    if (!live.isEmpty()) {
        return live;
    }
    const QString saved = settings.speechOutputName.isEmpty() ? QStringLiteral("Saved device")
                                                              : settings.speechOutputName;
    return QStringLiteral("%1 is not connected. Using the Windows default for now.").arg(saved);
}

} // namespace

void SettingsUi::decorateSpeechOutput(PageDocument& doc)
{
    if (PageCell* header = PageEdit::findCell(doc, QStringLiteral("h_output"))) {
        header->caption = outputCaption(m_settings);
    }
    if (PageGrid* list = PageEdit::findGrid(doc, QStringLiteral("out_list"))) {
        fillSpeechOutputList(*list);
    }
}

void SettingsUi::fillSpeechOutputList(PageGrid& host)
{
    struct Row {
        QString key;
        QString label;
        QString caption;
    };
    QVector<Row> rows;
    {
        Row def;
        def.label = QStringLiteral("System default");
        const QString name = AudioOutput::defaultPlaybackName();
        def.caption = name.isEmpty() ? QStringLiteral("No playback device found.") : name;
        rows.push_back(def);
    }
    bool saw = m_settings.speechOutputId.isEmpty();
    const QVector<AudioOutput::PlaybackDevice> devices = AudioOutput::playbackDevices();
    for (const AudioOutput::PlaybackDevice& dev : devices) {
        Row row;
        row.key = dev.key;
        row.label = dev.name;
        if (dev.isDefault) {
            row.caption = QStringLiteral("Windows default");
        }
        if (dev.key == m_settings.speechOutputId) {
            saw = true;
        }
        rows.push_back(row);
    }
    if (!saw) {
        Row missing;
        missing.key = m_settings.speechOutputId;
        missing.label = m_settings.speechOutputName.isEmpty() ? QStringLiteral("Saved device")
                                                              : m_settings.speechOutputName;
        missing.caption = QStringLiteral("Not connected");
        rows.push_back(missing);
    }

    int cols = 1;
    if (rows.size() > 8) {
        cols = 2;
    }
    if (rows.size() > 16) {
        cols = 3;
    }
    const int nRows = (rows.size() + cols - 1) / cols;
    host.rows = nRows;
    host.columns = cols;
    host.cells.clear();
    host.subGrids.clear();
    host.rowTracks.clear();
    host.columnTracks.clear();
    host.gapPx = 6;

    for (int i = 0; i < rows.size(); ++i) {
        const Row& row = rows[i];
        const QString cmd = QStringLiteral("settings.speech.output.%1")
                                .arg(VoiceCatalog::encodeId(row.key));
        PageCell c;
        c.id = row.key.isEmpty() ? QStringLiteral("out_default") : QStringLiteral("out_%1").arg(i);
        if (!saw && i == rows.size() - 1) {
            c.id = QStringLiteral("out_missing");
        }
        c.label = row.label;
        c.caption = row.caption;
        c.role = QStringLiteral("choice");
        c.row = i / cols;
        c.col = i % cols;
        c.activeState = cmd;
        c.styleId = QStringLiteral("plain");
        c.style.thickness = PageBox::all(1.0);
        c.style.radius = PageBox::all(12.0);
        PageAction action;
        action.type = PageActionType::Command;
        action.command = cmd;
        c.actions.push_back(action);
        host.cells.push_back(c);
    }
}

bool SettingsUi::selectSpeechOutput(const QString& encoded, QString* error)
{
    const QString key = VoiceCatalog::decodeId(encoded).trimmed();
    const QString live = key.isEmpty() ? QString() : AudioOutput::playbackName(key);
    if (!key.isEmpty() && live.isEmpty() && key != m_settings.speechOutputId) {
        if (error) {
            *error = QStringLiteral("That audio device is not connected");
        }
        return false;
    }
    if (m_settings.speechOutputId == key) {
        if (!live.isEmpty() && live != m_settings.speechOutputName) {
            m_settings.speechOutputName = live;
            apply(true);
            return true;
        }
        m_pages.refreshDecorated();
        return true;
    }
    m_settings.speechOutputId = key;
    m_settings.speechOutputName = live;
    apply(true);
    notifyStatus(key.isEmpty() ? QStringLiteral("Speech output: system default")
                               : QStringLiteral("Speech output: %1").arg(
                                     live.isEmpty() ? m_settings.speechOutputName : live));
    return true;
}

} // namespace gazer

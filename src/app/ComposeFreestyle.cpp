#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "assist/ElevenRequest.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QColor>
#include <QVector>

namespace gazer {

using compose_detail::ChoiceSlot;
using compose_detail::fillChoiceGrid;

void ComposeUi::fillFreestyleMode(PageDocument& doc) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor surface = theme.bgSurface.isValid() ? theme.bgSurface : QColor(40, 40, 44);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const bool editing = editMode();

    QVector<ChoiceSlot> voices;
    const auto& list = m_settings.savedSpeechVoices;
    const int n = qMin(AppSettings::kMaxSavedSpeechVoices, int(list.size()));
    voices.reserve(n);
    for (int i = 0; i < n; ++i) {
        const AppSettings::SavedSpeechVoice& v = list[i];
        ChoiceSlot s;
        s.label = v.name;
        if (qAbs(v.volume - 1.0) < 0.05) {
            s.caption = QString::number(v.speed, 'f', 1);
        } else {
            s.caption = QStringLiteral("%1  %2\u00d7")
                            .arg(v.speed, 0, 'f', 1)
                            .arg(v.volume, 0, 'f', 1);
        }
        s.icon = v.icon;
        s.color = v.color;
        s.useCommand = QStringLiteral("compose.voicePreset.%1").arg(v.id);
        s.editCommand = QStringLiteral("compose.editVoicePreset.%1").arg(v.id);
        s.activeState = QStringLiteral("compose.voicePreset.%1").arg(v.id);
        s.selected = editing && m_nameEdit == NameEditKind::Voice && m_editId == v.id;
        s.on = s.selected || (!editing && v.id == m_activeVoicePresetId);
        voices.push_back(s);
    }
    fillChoiceGrid(doc.findGrid(QStringLiteral("topics")), 1, 8, QStringLiteral("topic_"), voices,
                   editing, QStringLiteral("compose.newVoicePreset"), surface, accent, value);

    QVector<ChoiceSlot> tags;
    const auto& saved = m_settings.savedSpeechTags;
    tags.reserve(qMin(24, int(saved.size())));
    for (int i = 0; i < saved.size() && i < 24; ++i) {
        const AppSettings::SavedSpeechTag& tag = saved.at(i);
        ChoiceSlot s;
        s.label = QStringLiteral("[%1]").arg(tag.name);
        s.icon = tag.icon;
        s.color = tag.color;
        s.useCommand = QStringLiteral("compose.insertTagAt.%1").arg(i);
        s.editCommand = QStringLiteral("compose.editTag.%1").arg(i);
        s.selected = editing && m_nameEdit == NameEditKind::Tag && m_editId == QString::number(i);
        s.on = s.selected;
        tags.push_back(s);
    }
    fillChoiceGrid(doc.findGrid(QStringLiteral("soundboard")), 3, 8, QStringLiteral("tag_"), tags,
                   editing, QStringLiteral("compose.newTag"), surface, accent, value);
}

bool ComposeUi::applyVoicePreset(const QString& id, QString* error)
{
    const AppSettings::SavedSpeechVoice* found = nullptr;
    for (const AppSettings::SavedSpeechVoice& v : m_settings.savedSpeechVoices) {
        if (v.id == id) {
            found = &v;
            break;
        }
    }
    if (!found) {
        if (error) {
            *error = QStringLiteral("Unknown voice");
        }
        return false;
    }
    m_settings.speechModel = found->model;
    if (ElevenRequest::isElevenModel(ElevenRequest::normalizeModelId(found->model))) {
        m_settings.elevenVoiceId = found->voiceId;
    } else {
        m_settings.sapiVoiceToken = found->voiceId;
    }
    m_settings.speechSpeed = found->speed;
    m_settings.speechVolume = found->volume;
    m_activeVoicePresetId = found->id;
    apply();
    rebuildBoard();
    return true;
}

bool ComposeUi::editVoicePreset(const QString& id, QString* error)
{
    for (const AppSettings::SavedSpeechVoice& v : m_settings.savedSpeechVoices) {
        if (v.id == id) {
            return beginNameEdit(NameEditKind::Voice, id, v.name);
        }
    }
    if (error) {
        *error = QStringLiteral("Unknown voice");
    }
    return false;
}

bool ComposeUi::newVoicePreset(QString* error)
{
    if (m_settings.savedSpeechVoices.size() >= AppSettings::kMaxSavedSpeechVoices) {
        const QString msg = QStringLiteral("Voice row is full");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return true;
    }
    return beginNameEdit(NameEditKind::NewVoice, {}, ellipsis(currentVoiceDisplayName(), 24));
}

bool ComposeUi::editTagAt(int index, QString* error)
{
    if (index < 0 || index >= m_settings.savedSpeechTags.size()) {
        if (error) {
            *error = QStringLiteral("Unknown tag");
        }
        return false;
    }
    return beginNameEdit(NameEditKind::Tag, QString::number(index),
                         m_settings.savedSpeechTags.at(index).name);
}

bool ComposeUi::newTag(QString* error)
{
    if (m_settings.savedSpeechTags.size() >= 24) {
        const QString msg = QStringLiteral("Tag list is full");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return true;
    }
    return beginNameEdit(NameEditKind::NewTag, {}, {});
}

} // namespace gazer

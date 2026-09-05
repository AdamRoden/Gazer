#pragma once

#include "assist/ComposeBuffer.h"
#include "assist/VoiceCatalog.h"
#include "layout/PageTypes.h"

#include <QString>
#include <functional>

namespace gazer {

class AppSettings;
class ElevenClient;
class PageSession;
class PhraseService;
class SoundboardStore;
class SpeechHistory;
class SpeechEngine;
class SpeechSecrets;
class TtsService;

/// Gaze composer: capture Send/mapping on the compose page, stamp labels,
/// voice and history live boards.
class ComposeUi {
public:
    static constexpr auto kPageId = QLatin1String("compose");
    static constexpr auto kVoicesLiveId = QLatin1String("compose_voices_live");
    static constexpr auto kHistoryLiveId = QLatin1String("compose_history_live");
    static constexpr auto kItemEditLiveId = QLatin1String("compose_item_edit_live");

    ComposeUi(PageSession& pages, PhraseService& phrases, SpeechEngine& speech,
              AppSettings& settings, SpeechSecrets& secrets, ElevenClient& eleven,
              TtsService& tts, SoundboardStore& board, SpeechHistory& history);

    void setApplyFn(std::function<void()> fn) { m_apply = std::move(fn); }
    void setNotifyFn(std::function<void(const QString&)> fn) { m_notify = std::move(fn); }

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool assignMode() const;
    [[nodiscard]] bool editMode() const;
    [[nodiscard]] bool freestyleMode() const;
    [[nodiscard]] bool nameEditing() const;
    [[nodiscard]] bool editIconPalette() const { return m_editIcons; }
    [[nodiscard]] QString activeTopicId() const;
    [[nodiscard]] QString activeVoicePresetId() const { return m_activeVoicePresetId; }
    /// Consume Send and mapping/modifier Commands while compose (or a compose
    /// live board) is capturing. Composer builtins return false so dispatch runs them.
    [[nodiscard]] bool tryHandle(const PageAction& a, const QString& sourcePageId);

    void decoratePage(PageDocument& doc) const;

    ComposeBuffer& buffer() { return m_buffer; }
    [[nodiscard]] const ComposeBuffer& buffer() const { return m_buffer; }

    bool openCompose(QString* error);
    /// Reset assign/edit/name-edit when compose and the item-edit overlay are gone.
    void onSessionChanged();
    bool speakOrStop(QString* error);
    void stopSpeech();
    void clear();
    bool undo();
    bool redo();
    void backspace();
    void deleteWord();
    void removeVisibleWord(int slot);
    void insertTagAt(int index);
    void refresh();
    void refreshLiveBoards();
    /// Drop a Freestyle voice highlight that no longer matches live model/voice/speed.
    void syncFromSettings();
    void onCatalogReady(bool ok, const QString& error);

    bool openVoices(QString* error = nullptr);
    void setSpeechModel(const QString& model);
    void selectVoice(const QString& encodedId);
    void toggleFavorite();
    void previewCurrent();
    void voicesPage(int delta);
    void setGenderFilter(const QString& gender);
    void setLangFilter(const QString& language);
    void nudgeSpeed(int dir);

    bool toggleFreestyle(QString* error = nullptr);
    bool pin(QString* error = nullptr);
    void cancelAssign();
    bool editPins();
    bool saveNameEdit(QString* error = nullptr);
    void cancelNameEdit();
    bool deleteNameEdit(QString* error = nullptr);
    void showEditColors();
    void showEditIcons();
    bool setEditedColor(int index, QString* error = nullptr);
    bool setEditedIcon(int index, QString* error = nullptr);
    void clearEditedColor();
    void clearEditedIcon();
    bool applyVoicePreset(const QString& id, QString* error = nullptr);
    bool editVoicePreset(const QString& id, QString* error = nullptr);
    bool newVoicePreset(QString* error = nullptr);
    bool editTagAt(int index, QString* error = nullptr);
    bool newTag(QString* error = nullptr);
    bool openPinEdit(const QString& buttonId, QString* error = nullptr);
    bool openTopicEdit(const QString& topicId, QString* error = nullptr);
    bool playSoundboard(const QString& buttonId, QString* error = nullptr);
    bool assignSoundboard(const QString& cellOrButtonId, QString* error = nullptr);
    bool setTopic(const QString& topicId, QString* error = nullptr);
    bool newTopic(QString* error = nullptr);
    bool loadStarters(QString* error = nullptr);

    bool openHistory(QString* error = nullptr);
    bool playHistory(const QString& id, QString* error = nullptr);
    bool restoreHistory(const QString& id, QString* error = nullptr);
    bool deleteHistory(const QString& id, QString* error = nullptr);
    void historyPage(int delta);
    void historyGoto(int offset);
    void onHistoryReady(const QString& phrase, const QString& backend, const QString& modelId,
                        const QString& voiceId, const QString& mpegPath);

private:
    [[nodiscard]] bool isCapturing(const QString& sourcePageId) const;
    void handleSend(const PageAction& a);
    void handleCapturedCommand(const QString& command);
    void insertText(QStringView chars);
    void apply();
    void notify(const QString& msg);
    static QString ellipsis(const QString& text, int maxChars);
    static bool isAllowThroughCommand(const QString& name);
    enum class BoardKind { Topics, Freestyle };
    enum class Interaction { Use, Assign, Edit, NameEdit };
    enum class NameEditKind { None, Topic, Pin, Voice, NewVoice, Tag, NewTag };

    void rebuildBoard();
    void fillSoundboard(PageDocument& doc) const;
    void fillTopicsMode(PageDocument& doc) const;
    void fillFreestyleMode(PageDocument& doc) const;
    void stampComposerChrome(PageDocument& doc) const;
    void closeItemEdit();
    bool beginNameEdit(NameEditKind kind, const QString& target, const QString& initial);
    void endNameEdit(bool restorePhrase);
    void finishItemEditor(bool restorePhrase);
    void abandonClosedSession();
    void syncClosedOverlays();
    void loadEditAppearance();
    void persistEditAppearance();
    void syncActiveVoicePreset();
    bool presentItemEdit(QString* error = nullptr);
    void rebuildItemEdit();
    [[nodiscard]] bool hasLive(QLatin1String id) const;
    [[nodiscard]] bool nameEditCanDelete() const;
    [[nodiscard]] int editedVoiceIndex() const;
    [[nodiscard]] int editedTagIndex() const;
    [[nodiscard]] QString currentVoiceDisplayName() const;
    PageDocument buildItemEditDocument() const;

    [[nodiscard]] bool elevenMode() const;
    [[nodiscard]] QVector<VoiceCatalog::Voice> currentVoices() const;
    [[nodiscard]] QVector<VoiceCatalog::Voice> filteredVoices() const;
    [[nodiscard]] QString currentVoiceId() const;
    PageDocument buildVoicesDocument();
    bool presentLive(const QString& id, PageDocument doc, QString* error);
    void rebuildVoices();
    void requestCatalogIfNeeded();

    PageDocument buildHistoryDocument();
    void rebuildHistory();

    PageSession& m_pages;
    PhraseService& m_phrases;
    SpeechEngine& m_speech;
    AppSettings& m_settings;
    SpeechSecrets& m_secrets;
    ElevenClient& m_eleven;
    TtsService& m_tts;
    SoundboardStore& m_board;
    SpeechHistory& m_history;
    ComposeBuffer m_buffer;
    std::function<void()> m_apply;
    std::function<void(const QString&)> m_notify;

    bool m_voicesLoading = false;
    QString m_genderFilter;
    int m_voicePage = 0;
    BoardKind m_boardKind = BoardKind::Topics;
    Interaction m_interaction = Interaction::Use;
    NameEditKind m_nameEdit = NameEditKind::None;
    QString m_editId;
    QString m_nameEditBackup;
    bool m_editIcons = false;
    QString m_editColor;
    QString m_editIcon;
    QString m_activeVoicePresetId;
    int m_historyPage = 0;
};

} // namespace gazer

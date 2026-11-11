#pragma once

#include "assist/ComposeBuffer.h"
#include "assist/GazeTrack.h"
#include "assist/SoundboardStore.h"
#include "assist/VoiceCatalog.h"
#include "assist/VoiceStyles.h"
#include "core/GazePoint.h"
#include "predict/WordPredictor.h"
#include "layout/InvalidGazeGrace.h"
#include "layout/PageTypes.h"

#include <QByteArray>
#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>
#include <functional>
#include <memory>

namespace gazer {

class AppSettings;
class ElevenClient;
class PageSession;
class TypingContext;
class SpeechHistory;
class SpeechEngine;
class SpeechSecrets;
class TtsService;
class IconCatalog;

/// Gaze composer: capture Send/mapping on the compose page, stamp labels,
/// voice and history live boards.
class ComposeUi {
public:
    static constexpr auto kPageId = QLatin1String("compose");
    static constexpr auto kVoicesLiveId = QLatin1String("compose_voices_live");
    static constexpr auto kHistoryLiveId = QLatin1String("compose_history_live");
    static constexpr auto kFilesLiveId = QLatin1String("compose_files_live");
    static constexpr auto kItemEditLiveId = QLatin1String("compose_item_edit_live");

    ComposeUi(PageSession& pages, SpeechEngine& speech, AppSettings& settings,
              SpeechSecrets& secrets, ElevenClient& eleven, TtsService& tts,
              SoundboardStore& board, SpeechHistory& history);
    ~ComposeUi();

    void setApplyFn(std::function<void()> fn) { m_apply = std::move(fn); }
    void setNotifyFn(std::function<void(const QString&)> fn) { m_notify = std::move(fn); }
    /// Color on the item editor opens the settings color picker. Save calls the commit.
    void setColorPickerFns(std::function<bool(const QColor& initial, const QString& title,
                                               std::function<void(const QColor&)> commit,
                                               QString* error)>
                               open,
                           std::function<void()> dismissGuest)
    {
        m_colorPick = std::move(open);
        m_dismissColor = std::move(dismissGuest);
    }
    void setTypingContext(TypingContext* typing);
    [[nodiscard]] WordPredictor& predictor() { return m_predictor; }

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool assignMode() const;
    [[nodiscard]] bool editMode() const;
    [[nodiscard]] bool nameEditing() const;
    [[nodiscard]] bool editIconPalette() const { return m_editIcons; }
    [[nodiscard]] QString activeTopicId() const;
    [[nodiscard]] QString currentVoiceId() const;
    /// Consume Send and mapping/modifier Commands while compose (or a compose
    /// live board) is capturing. Composer builtins return false so dispatch runs them.
    [[nodiscard]] bool tryHandle(const PageAction& a, const QString& sourcePageId);

    void decoratePage(PageDocument& doc) const;

    ComposeBuffer& buffer() { return m_buffer; }
    [[nodiscard]] const ComposeBuffer& buffer() const { return m_buffer; }

    bool openCompose(QString* error);
    /// Reset assign/edit/name-edit when Speak and its live boards are gone.
    void onSessionChanged();
    bool speakOrStop(QString* error);
    void stopSpeech();
    void clear();
    bool undo();
    bool redo();
    void backspace();
    void deleteWord();
    void selectAll();
    void cut();
    void copy();
    void paste();
    void pageChips(int delta);
    void applyStyle(const QString& encoded);
    void clearStyle();
    void openStyleEditor();
    void closeStyleEditor();
    void pageStyles(int delta);
    void toggleStyleFavorite(const QString& encoded);
    void trashStyle(const QString& encoded);
    void beginStyleName();
    void insertSavedTag(const QString& encoded);
    void pageTags(int delta);
    void beginTagName();
    void removeVisibleWord(int slot);
    void acceptPrediction(int slot);
    void moveEndOfWord(int slot);
    void moveStartOfWord(int slot);
    void refresh();
    void refreshLiveBoards();
    /// Refresh the open Speak board after settings change.
    void syncFromSettings();
    void onCatalogReady(bool ok, const QString& error);

    bool openVoices(QString* error = nullptr);
    void setSpeechModel(const QString& model);
    void selectVoice(const QString& encodedId);
    void toggleFavorite(const QString& encodedId = {});
    void previewCurrent();
    void previewVoice(const QString& encodedId);
    void voicesPage(int delta);
    void voicesGoto(int offset);
    void setGenderFilter(const QString& gender);
    void setLangFilter(const QString& language);
    void toggleLangBrowse();
    void toggleVoiceSearch();
    void nudgeSpeed(int dir);
    void nudgePitch(int dir);
    void nudgeVolume(int dir);

    bool pin(QString* error = nullptr);
    void cancelAssign();
    bool editPins();
    bool saveNameEdit(QString* error = nullptr);
    void cancelNameEdit();
    bool deleteNameEdit(QString* error = nullptr);
    /// Settings color picker. The chosen color stays on the item until Save.
    void showEditColors();
    /// Searchable Material Symbols board, the same size as the color picker.
    /// The footer is Clear, the current icon, Save, and Cancel. Save applies
    /// the icon. Cancel discards it. Tiles move one row at a time.
    void showEditIcons();
    /// Dwell on Search. Shows the Speak keyboard until Enter or Esc.
    void focusIconSearch();
    bool pickIcon(int index, QString* error = nullptr);
    void saveIconPick();
    void cancelIconPick();
    /// Move the icon tiles by `rows` (one row is eight tiles).
    void scrollIconRows(int rows);
    /// Jump to tile row `row`. The scrollbar offset is a row, not an icon.
    void iconsGoto(int row);
    /// Export browser. Same board as import, with the file-name editor.
    bool exportBoard(QString* error = nullptr);
    /// Import/merge browser.
    bool importChoose(QString* error = nullptr);
    /// Import/merge browser. The actions grid has no separate File Explorer button.
    bool openFiles(QString* error = nullptr);
    bool pickImportFile(int index, QString* error = nullptr);
    void pageImportFiles(int delta);
    /// Move the file list by `rows`. A list row is one file. A tile row is eight.
    void scrollFileRows(int rows);
    /// Move the export file-name caret by `delta` characters.
    void nudgeFileCaret(int delta);
    /// List shows the detail columns. Tiles shows a grid of icons.
    void setFileTiles(bool tiles);
    bool browserUp(QString* error = nullptr);
    bool browserPrev(QString* error = nullptr);
    bool browserForward(QString* error = nullptr);
    bool browserGoto(int index, QString* error = nullptr);
    /// Open a Places entry. Empty path is This PC.
    bool browserPlace(int index, QString* error = nullptr);
    bool browserSave(QString* error = nullptr);
    void browserBack();
    /// Yes on the File Explorer confirm row. Reads or writes only here.
    bool confirmFileYes(QString* error = nullptr);
    void confirmFileNo();
    void filesGoto(int offset);
    bool importMerge(QString* error = nullptr);
    bool importReplace(QString* error = nullptr);
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
                        const QString& voiceId, const QString& clipPath);
    /// Drag a list scrollbar, or the export file-name caret, after the lock-on ring.
    void onGaze(const GazePoint& point);

private:
    [[nodiscard]] bool isCapturing(const QString& sourcePageId) const;
    /// Phrase buffer holds a topic, pin, style, or tag name.
    [[nodiscard]] bool capturingPhrase() const;
    [[nodiscard]] bool borrowingPhrase() const;
    /// Put the phrase back. Does not change the keyboard layer.
    void endBorrowedPhrase();
    /// Put the phrase back and return to the style or tag layer.
    void cancelBorrowedPhrase();
    bool saveBorrowedPhrase(QString* error);
    [[nodiscard]] bool handleVoiceQuerySend(const PageAction& action);
    [[nodiscard]] bool handleVoiceQueryCommand(const QString& command);
    [[nodiscard]] bool handleIconQuerySend(const PageAction& action);
    [[nodiscard]] bool handleIconQueryCommand(const QString& command);
    [[nodiscard]] bool handleVoicesEscape();
    void clearVoiceQuery();
    void handleSend(const PageAction& a);
    void handleCapturedCommand(const QString& command);
    void insertText(QStringView chars);
    void apply();
    void notify(const QString& msg);
    static QString ellipsis(const QString& text, int maxChars);
    static bool isAllowThroughCommand(const QString& name);
    enum class Interaction { Use, Assign, Edit, NameEdit };
    enum class NameEditKind { None, Topic, Pin };

    void rebuildBoard();
    [[nodiscard]] PageDim speakBoardWidth() const;
    void fillSoundboard(PageDocument& doc) const;
    void fillTopicsMode(PageDocument& doc) const;
    void stampChips(PageDocument& doc) const;
    void stampStyleLayer(PageDocument& doc) const;
    void stampTagLayer(PageDocument& doc) const;
    void ensureBrowserEntries() const;
    [[nodiscard]] bool editingFileName() const;
    /// Phrase buffer, or the file name while the file browser is the top page.
    [[nodiscard]] ComposeBuffer& editTarget();
    void insertFileName(QStringView chars);
    /// Send, mapping keys, and Escape while the file name is being edited.
    [[nodiscard]] bool handleFileSend(const PageAction& action);
    [[nodiscard]] bool handleFileCommand(const QString& command);
    [[nodiscard]] bool handleFileEscape();
    [[nodiscard]] bool consumeFileSpeak(QString* error);
    void rebuildFiles();
    void navigateBrowser(const QString& dir);
    PageDocument buildFilesDocument() const;
    enum class FileConfirm { None, Import, Merge, Export };
    enum class FileMode { Import, Export };
    bool openFileMode(FileMode mode, QString* error);
    bool armFileConfirm(FileConfirm kind, QString* error);
    bool requestFileImport(bool replace, QString* error);
    bool writeExport(QString* error);
    bool writeWorkspaceFile(const QString& path, QString* error);
    bool loadPendingFile(const QString& path, QString* error);
    bool readSelectedWorkspace(QString* error);
    void restoreFileLayers();
    [[nodiscard]] QString selectedJsonPath() const;
    [[nodiscard]] QString fileConfirmQuestion() const;
    /// `keep` survives closing File Explorer. Edits drop either kind.
    void setBoardNote(const QString& text, bool keep = false);
    void dismissBoardNote();
    void commitEdit();
    void stampStylePicker(PageGrid& grid) const;
    void stampStyleEditor(PageGrid& grid) const;
    void stampComposerChrome(PageDocument& doc) const;
    void presentNamingPhrase(PageCell& cell) const;
    void showComposeLayer(int layer);
    bool saveStyleName(QString* error = nullptr);
    void cancelStyleName();
    bool saveTagName(QString* error = nullptr);
    void cancelTagName();
    void exitVoiceSearch();
    void storeStyleCatalog(const VoiceStyles::Catalog& catalog);
    [[nodiscard]] VoiceStyles::Catalog styleCatalog() const;
    bool importPending(bool replace, QString* error);
    void applyImportedSpeech(const SoundboardStore::WorkspaceImportedSettings& imported);
    void updatePredictions();
    void layoutPredictRow(PageDocument& doc) const;
    void loadPredictor();
    void closeItemEdit();
    /// Drop the icon board, the draft, the query, and the search keyboard.
    void endIconPick();
    void clearIconSession();
    void closeIconKeyboard();
    [[nodiscard]] bool handleIconKeyboardEscape();
    void onIconCatalogChanged();
    void onIconSvgsReady();
    [[nodiscard]] QStringList iconHits() const;
    void prefetchVisibleIcons();
    void prefetchBoardIcons();
    void prefetchIcons(const QStringList& names);
    bool beginNameEdit(NameEditKind kind, const QString& target, const QString& initial);
    void endNameEdit(bool restorePhrase);
    void finishItemEditor(bool restorePhrase);
    void abandonClosedSession();
    void syncClosedOverlays();
    void loadEditAppearance();
    void persistEditAppearance();
    bool presentItemEdit(QString* error = nullptr);
    void rebuildItemEdit();
    [[nodiscard]] bool hasLive(QLatin1String id) const;
    [[nodiscard]] bool nameEditCanDelete() const;
    PageDocument buildItemEditDocument() const;

    [[nodiscard]] bool elevenMode() const;
    [[nodiscard]] QVector<VoiceCatalog::Voice> currentVoices() const;
    [[nodiscard]] QVector<VoiceCatalog::Voice> filteredVoices() const;
    PageDocument buildVoicesDocument();
    bool presentLive(const QString& id, PageDocument doc, QString* error);
    void rebuildVoices();
    void requestCatalogIfNeeded();

    PageDocument buildHistoryDocument();
    void rebuildHistory();
    void feedListScrollGaze(const GazePoint& point);
    void stopListScroll();

    PageSession& m_pages;
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
    std::function<bool(const QColor&, const QString&, std::function<void(const QColor&)>, QString*)>
        m_colorPick;
    std::function<void()> m_dismissColor;

    bool m_voicesLoading = false;
    QString m_genderFilter;
    int m_voicePage = 0;
    Interaction m_interaction = Interaction::Use;
    NameEditKind m_nameEdit = NameEditKind::None;
    QString m_editId;
    QString m_nameEditBackup;
    /// Icon list is open. The phrase keeps `m_editIcon` until Save.
    bool m_editIcons = false;
    QString m_editColor;
    QString m_editIcon;
    QString m_iconDraft;
    /// First visible tile row. Cell 0 of that row is `m_iconRow * 8`.
    int m_iconRow = 0;
    QString m_iconQuery;
    /// Search owns the Speak keyboard. Esc is in the Edit key's place.
    bool m_iconKeyboard = false;
    std::unique_ptr<IconCatalog> m_iconCatalog;
    QByteArray m_pendingImport;
    QString m_pendingImportName;
    QString m_pendingImportPath;
    /// Empty directory means the drive list. File Explorer reads and writes this folder.
    QString m_browserDir;
    QString m_boardNote;
    /// Result notes stay on the phrase after File Explorer closes.
    bool m_boardNoteKeep = false;
    mutable QStringList m_browserEntries;
    mutable bool m_browserFresh = false;
    /// First visible File Explorer item.
    int m_fileOffset = 0;
    /// Tile grid instead of the detail columns. List is the default.
    bool m_fileTiles = false;
    FileConfirm m_fileConfirm = FileConfirm::None;
    FileMode m_fileMode = FileMode::Import;
    /// Speak layers under File Explorer. Restored when it closes.
    QVector<int> m_fileRestoreLayers;
    QStringList m_fileBack;
    QStringList m_fileForward;
    /// Export name. The Speak keyboard types into this while the file browser is on top.
    ComposeBuffer m_fileName;
    int m_historyPage = 0;
    bool m_styleEditor = false;
    /// Phrase buffer is borrowed for a style or tag name. The two never overlap.
    enum class BorrowedPhrase { None, Style, Tag };
    BorrowedPhrase m_borrowed = BorrowedPhrase::None;
    int m_borrowCaret = 0;
    int m_tagPage = 0;
    int m_stylePage = 0;
    /// Voice-list name query. Typed on the Speak keyboard, not into the phrase.
    bool m_voiceSearch = false;
    bool m_langBrowse = false;
    QString m_voiceQuery;
    /// Styles shown on one editor page: 2 rows × 3 columns.
    static constexpr int kStyleEditorPage = 6;
    enum class ListScroll { None, History, Voices, Files, FileName, Icons };
    ListScroll m_listScroll = ListScroll::None;
    mutable PageDocument m_composeAuthored;
    struct ShownPrediction {
        QString label;
        QString insertText;
        int start = 0;
        int end = 0;
        QString learnWord;
        QStringList learnLeft;
    };
    QVector<ShownPrediction> m_shown;
    WordPredictor m_predictor;
    TypingContext* m_typing = nullptr;
    bool m_predictWarned = false;
    InvalidGazeGrace m_listScrollGrace;
    QElapsedTimer m_listScrollClock;
    GazeTrack::Lock m_listScrollLock;
    GazeTrack::Drag m_listScrollDrag;
};

} // namespace gazer

#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace gazer {

class SpeechHistory;

struct SoundboardButton {
    QString id;
    QString label;
    QString icon;
    QString color;
    QString sourceText;
    QString utteranceText;
    QString clipId;
    bool effectsBaked = false;
    int col = 0;
    int row = 0;
    int colSpan = 1;
    int rowSpan = 1;
};

struct SoundboardTopic {
    QString id;
    QString name;
    QString icon;
    QString color;
    int gridCols = 4;
    int gridRows = 3;
    QVector<SoundboardButton> buttons;
};

/// JSON soundboard + on-disk MPEG clips. No UI.
class SoundboardStore {
public:
    static constexpr int kMaxCols = 4;
    static constexpr int kMaxRows = 6;
    static constexpr qint64 kMaxTotalBytes = 200ll * 1024 * 1024;
    static constexpr qint64 kMaxClipBytes = 5ll * 1024 * 1024;

    struct PlayPlan {
        enum class Kind { None, Utterance, Clip, Source };
        Kind kind = Kind::None;
        QString text;
        QString clipPath;
    };

    explicit SoundboardStore(QString rootDir = {});

    [[nodiscard]] QString rootDir() const { return m_root; }
    [[nodiscard]] QString boardsPath() const;
    [[nodiscard]] QString clipsDir() const;

    bool load(QString* error = nullptr);
    bool save(QString* error = nullptr) const;

    [[nodiscard]] const QVector<SoundboardTopic>& topics() const { return m_topics; }
    [[nodiscard]] QString activeTopicId() const { return m_activeTopicId; }
    [[nodiscard]] SoundboardTopic* activeTopic();
    [[nodiscard]] const SoundboardTopic* activeTopic() const;

    bool setActiveTopic(const QString& id);
    bool newTopic(QString* error = nullptr);
    bool removeTopic(const QString& id, QString* error = nullptr);
    bool setTopicColor(const QString& id, const QString& color, QString* error = nullptr);
    bool setTopicIcon(const QString& id, const QString& icon, QString* error = nullptr);
    bool renameTopic(const QString& id, const QString& name, QString* error = nullptr);

    [[nodiscard]] SoundboardTopic* findTopic(const QString& id);
    [[nodiscard]] const SoundboardTopic* findTopic(const QString& id) const;
    /// Fill the active topic with everyday starters if it has no buttons.
    bool loadStarters(QString* error = nullptr);

    [[nodiscard]] SoundboardButton* buttonAt(SoundboardTopic& topic, int row, int col);
    [[nodiscard]] const SoundboardButton* buttonAt(const SoundboardTopic& topic, int row,
                                                   int col) const;
    [[nodiscard]] SoundboardButton* findButton(const QString& buttonId);
    [[nodiscard]] const SoundboardButton* findButton(const QString& buttonId) const;

    /// Create or overwrite the button at `row,col` on the active topic.
    bool assignCell(int row, int col, const SoundboardButton& seed, QString* error = nullptr);
    bool removeButton(const QString& buttonId, QString* error = nullptr);
    bool setButtonColor(const QString& buttonId, const QString& color, QString* error = nullptr);
    bool setButtonIcon(const QString& buttonId, const QString& icon, QString* error = nullptr);
    bool setButtonLabel(const QString& buttonId, const QString& label, QString* error = nullptr);

    [[nodiscard]] PlayPlan planPlay(const SoundboardButton& b) const;
    [[nodiscard]] QString clipPath(const QString& clipId) const;
    [[nodiscard]] static bool validClipId(const QString& clipId);

    /// Copy `srcPath` into clips/. Returns the new clip id.
    /// Evicts oldest history (via `history`) then unreferenced clips before refusing at cap.
    QString importClip(const QString& srcPath, QString* error = nullptr,
                       SpeechHistory* history = nullptr);
    void evictUnreferencedClips();
    void setMaxTotalBytes(qint64 n);

    static QVector<SoundboardTopic> starterTopics();
    static void clampTopic(SoundboardTopic* topic);
    static void repackSequential(SoundboardTopic* topic);

private:
    [[nodiscard]] QStringList referencedClipIds() const;
    [[nodiscard]] qint64 usedBytes() const;
    static QString makeId();

    QString m_root;
    QString m_activeTopicId;
    QVector<SoundboardTopic> m_topics;
    qint64 m_maxBytes = kMaxTotalBytes;
};

} // namespace gazer

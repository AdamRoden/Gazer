#pragma once

#include "predict/CharPrior.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace gazer {

class WordPredictor;
struct PageAction;

/// What Gazer just typed, on any keyboard page. Speak uses the phrase. Every other
/// board appends the keys it actually sends. One character prior and one dictionary.
class TypingContext final : public QObject {
    Q_OBJECT

public:
    explicit TypingContext(WordPredictor* words, QObject* parent = nullptr);
    ~TypingContext() override;

    void setListener(std::function<void(const QHash<QChar, double>&)> fn);

    /// Shipped `charprior.bin`, personal character counts, and `predict-user.bin`.
    bool load(const QString& shippedPath, const QString& userCharPath, const QString& userWordPath,
              QString* error = nullptr);

    /// OS keyboard and any non-Speak page. A following key after Speak starts a new stream
    /// so the other app is not conditioned on the phrase.
    void noteKey(const QString& pageId, QChar symbol);
    void noteBackspace(const QString& pageId);
    void noteSend(const QString& pageId, const QString& key, const QString& edge);
    void noteCommand(const QString& pageId, const QString& command);
    /// Successful page action. ShowLayers, speech, and mouse moves leave the stream alone.
    void noteAction(const QString& pageId, const PageAction& action);

    /// Speak phrase up to the caret. Learns the edit and replaces the stream.
    void syncPhrase(const QString& text, int caret);
    /// Put the phrase in place without counting it. Name-edit restore uses this.
    void adoptPhrase(const QString& text, int caret);
    /// Speak accepted a dictionary word. Saves the personal word file.
    void noteAcceptedWord(const QStringList& left, const QString& word);

    /// Arrows, clicks, navigation: drop the stream. Counts stay.
    void resetContext();
    /// Two seconds of silence: stop conditioning, keep the string so the next edit still diffs.
    void forgetContext();

    [[nodiscard]] CharPrior& characters() { return m_prior; }
    [[nodiscard]] const CharPrior& characters() const { return m_prior; }
    [[nodiscard]] QString context() const { return m_context; }
    [[nodiscard]] bool contextStale() const { return m_stale; }
    [[nodiscard]] QHash<QChar, double> distribution() const;

private:
    enum class Source { Keys, Phrase };

    [[nodiscard]] static bool isComposePage(const QString& pageId);
    [[nodiscard]] static QString normalizePrefix(const QString& text);
    void appendSymbol(QChar symbol, bool learn);
    void chopLearned();
    void closeTokenEndingAt(int endExclusive);
    void applyDiff(const QString& next);
    void noteLineBreak();
    void noteNamedKey(const QString& pageId, const QString& name, bool isSend);
    void saveUsers();
    void publish();
    void armIdle();

    WordPredictor* m_words = nullptr;
    CharPrior m_prior;
    QString m_context;
    struct Undo {
        ushort prev2 = 0;
        ushort prev1 = 0;
        ushort next = 0;
    };
    QVector<Undo> m_undo;
    Source m_source = Source::Keys;
    bool m_stale = false;
    bool m_charDirty = false;
    bool m_wordDirty = false;
    QString m_userCharPath;
    QString m_userWordPath;
    QTimer m_idle;
    std::function<void(const QHash<QChar, double>&)> m_listener;
};

} // namespace gazer

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;

namespace gazer {

/// One HTTP PCM stream per breath. The next request starts while the current
/// body is still downloading. GUI thread only.
class ElevenHttpStream final : public QObject {
    Q_OBJECT

public:
    explicit ElevenHttpStream(QNetworkAccessManager* nam, QObject* parent = nullptr);

    void start(const QString& apiKey, const QString& voiceId, const QStringList& phrases);
    void abort();

signals:
    void speechChunk(const QByteArray& pcm);
    void speechStreamEnded();
    void speechFailed(int httpStatus, const QString& error, int retryAfterMs);

private:
    class ReplySlot;
    void armStall();
    void drop(ReplySlot*& slot);
    ReplySlot* openPhrase(int index);
    void maybePrefetch();
    void finishLive();
    void openLiveOrSucceed();
    void fail(int httpStatus, const QString& error, int retryAfterMs);
    void succeed();
    void onReady(ReplySlot* slot);
    void onDone(ReplySlot* slot);

    QNetworkAccessManager* m_nam = nullptr;
    QTimer* m_stall = nullptr;
    QString m_key;
    QString m_voiceId;
    QStringList m_phrases;
    int m_openAt = 0;
    ReplySlot* m_live = nullptr;
    ReplySlot* m_ahead = nullptr;
    bool m_settled = true;
    bool m_heard = false;
};

} // namespace gazer

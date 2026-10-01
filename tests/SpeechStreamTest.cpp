#include "assist/DialogueSocket.h"
#include "assist/PcmRate.h"
#include "assist/PcmWav.h"
#include "assist/WsFrame.h"

#include <QtTest>

using namespace gazer;

class SpeechStreamTest final : public QObject {
    Q_OBJECT

private slots:
    void wsRoundTrip();
    void wsNeedMoreAndRsv();
    void pcmIdentity();
    void pcmDoubleSpeed();
    void pcmHalfSpeed();
    void wavHeader();
    void dialogueUpgrade();
    void dialogueEvent();
};

void SpeechStreamTest::wsRoundTrip()
{
    const QByteArray payload("hello");
    const QByteArray frame = WsFrame::clientFrame(WsFrame::kOpText, payload, 0x01020304u);
    const WsFrame::Incoming in = WsFrame::serverFrame(frame);
    QCOMPARE(int(in.status), int(WsFrame::Incoming::Status::Ok));
    QVERIFY(in.fin);
    QCOMPARE(in.opcode, WsFrame::kOpText);
    QCOMPARE(in.payload, payload);
    QCOMPARE(in.consumed, frame.size());

    QByteArray longPayload(200, 'a');
    const QByteArray longFrame = WsFrame::clientFrame(WsFrame::kOpText, longPayload, 0xA1B2C3D4u);
    const WsFrame::Incoming longIn = WsFrame::serverFrame(longFrame);
    QCOMPARE(longIn.payload, longPayload);
    QVERIFY(longFrame.size() > 200);
}

void SpeechStreamTest::wsNeedMoreAndRsv()
{
    const WsFrame::Incoming partial = WsFrame::serverFrame(QByteArray(1, char(0x81)));
    QCOMPARE(int(partial.status), int(WsFrame::Incoming::Status::NeedMore));
    QCOMPARE(partial.consumed, 0);

    const WsFrame::Incoming bad = WsFrame::serverFrame(QByteArray("\xC1\x00", 2));
    QCOMPARE(int(bad.status), int(WsFrame::Incoming::Status::Error));

    QByteArray ping(2, '\0');
    ping[0] = char(0x89);
    ping[1] = char(0x00);
    const WsFrame::Incoming pongish = WsFrame::serverFrame(ping);
    QCOMPARE(pongish.opcode, WsFrame::kOpPing);
    QVERIFY(pongish.fin);
    QVERIFY(pongish.payload.isEmpty());
}

void SpeechStreamTest::pcmIdentity()
{
    QByteArray pcm;
    const qint16 samples[] = {0, 1000, -1000, 32000};
    for (qint16 s : samples) {
        pcm.append(reinterpret_cast<const char*>(&s), 2);
    }
    PcmRate::Resampler rate;
    QByteArray out = rate.process(pcm, 1.0);
    out += rate.flush(1.0);
    QCOMPARE(out, pcm);
}

void SpeechStreamTest::pcmDoubleSpeed()
{
    QByteArray pcm;
    const qint16 samples[] = {0, 100, 200, 300};
    for (qint16 s : samples) {
        pcm.append(reinterpret_cast<const char*>(&s), 2);
    }
    PcmRate::Resampler rate;
    QByteArray out = rate.process(pcm, 2.0);
    out += rate.flush(2.0);
    QCOMPARE(out.size(), 4);
    const auto* played = reinterpret_cast<const qint16*>(out.constData());
    QCOMPARE(played[0], qint16(0));
    QCOMPARE(played[1], qint16(200));
}

void SpeechStreamTest::pcmHalfSpeed()
{
    QByteArray pcm;
    const qint16 samples[] = {0, 100, 200, 300};
    for (qint16 s : samples) {
        pcm.append(reinterpret_cast<const char*>(&s), 2);
    }
    PcmRate::Resampler rate;
    QByteArray out = rate.process(pcm, 0.5);
    out += rate.flush(0.5);
    QCOMPARE(out.size(), 16);
}

void SpeechStreamTest::wavHeader()
{
    const QByteArray pcm(4, '\x11');
    const QByteArray wav = PcmWav::wrapPcm16Mono(pcm, PcmWav::kSampleRate);
    QCOMPARE(wav.size(), 48);
    QCOMPARE(wav.left(4), QByteArray("RIFF"));
    QCOMPARE(wav.mid(8, 4), QByteArray("WAVE"));
    QCOMPARE(wav.mid(36, 4), QByteArray("data"));
    QCOMPARE(int(uchar(wav.at(24))), PcmWav::kSampleRate & 0xFF);
    QCOMPARE(wav.right(4), pcm);
}

void SpeechStreamTest::dialogueUpgrade()
{
    const DialogueUpgrade partial = parseDialogueUpgrade(QByteArrayLiteral("HTTP/1.1 101"));
    QCOMPARE(int(partial.status), int(DialogueUpgrade::Status::NeedMore));

    QByteArray huge(16385, 'H');
    const DialogueUpgrade hugeUp = parseDialogueUpgrade(huge);
    QCOMPARE(int(hugeUp.status), int(DialogueUpgrade::Status::Failed));

    const DialogueUpgrade denied = parseDialogueUpgrade(
        QByteArrayLiteral("HTTP/1.1 401 Unauthorized\r\n\r\n"));
    QCOMPARE(int(denied.status), int(DialogueUpgrade::Status::Failed));
    QCOMPARE(denied.httpStatus, 401);
    QCOMPARE(denied.error, QStringLiteral("Invalid API key"));

    const DialogueUpgrade busy = parseDialogueUpgrade(
        QByteArrayLiteral("HTTP/1.1 429 Too Many\r\nRetry-After: 2\r\n\r\n"));
    QCOMPARE(busy.httpStatus, 429);
    QCOMPARE(busy.retryAfterMs, 2000);
    QCOMPARE(busy.error, QStringLiteral("ElevenLabs busy — try again"));

    const DialogueUpgrade ready = parseDialogueUpgrade(
        QByteArrayLiteral("HTTP/1.1 101 Switching Protocols\r\n\r\n{\"audio\":"));
    QCOMPARE(int(ready.status), int(DialogueUpgrade::Status::Ready));
    QCOMPARE(ready.rest, QByteArrayLiteral("{\"audio\":"));
}

void SpeechStreamTest::dialogueEvent()
{
    const QByteArray pcm("\x00\x01", 2);
    const QByteArray json =
        QByteArrayLiteral("{\"audio\":\"") + pcm.toBase64() + QByteArrayLiteral("\",\"is_final\":true}");
    const DialogueEvent ev = parseDialogueEvent(json);
    QVERIFY(!ev.failed);
    QVERIFY(ev.isFinal);
    QCOMPARE(ev.pcm, pcm);

    const DialogueEvent auth =
        parseDialogueEvent(QByteArrayLiteral("{\"error\":\"unauthorized\",\"message\":\"no\"}"));
    QVERIFY(auth.failed);
    QCOMPARE(auth.httpStatus, 401);
    QCOMPARE(auth.error, QStringLiteral("Invalid API key"));
    QVERIFY(auth.pcm.isEmpty());

    const DialogueEvent rate =
        parseDialogueEvent(QByteArrayLiteral("{\"error\":\"rate_limit\",\"message\":\"slow\"}"));
    QVERIFY(rate.failed);
    QCOMPARE(rate.httpStatus, 429);
    QCOMPARE(rate.retryAfterMs, 1000);

    const DialogueEvent junk = parseDialogueEvent(QByteArrayLiteral("nope"));
    QVERIFY(!junk.failed);
    QVERIFY(!junk.isFinal);
    QVERIFY(junk.pcm.isEmpty());
}

QObject* createSpeechStreamTest()
{
    return new SpeechStreamTest;
}

#include "SpeechStreamTest.moc"

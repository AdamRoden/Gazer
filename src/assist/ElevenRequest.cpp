#include "assist/ElevenRequest.h"

#include "assist/PcmWav.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace ElevenRequest {
namespace {

const QRegularExpression& tagRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\[[^\]]*\])"));
    return re;
}

double clampOrDefault(double v, double lo, double hi, double fallback)
{
    if (!std::isfinite(v)) {
        v = fallback;
    }
    return std::clamp(v, lo, hi);
}

bool isLegacyEleven(const QString& s)
{
    return s == kModelTurbo || s == QLatin1String("eleven_v3")
           || s == QLatin1String("eleven_flash_v2_5") || s == QLatin1String("eleven_flash_v2")
           || s == QLatin1String("eleven_multilingual_v2") || s == QLatin1String("eleven_v4");
}

QByteArray jsonBytes(const QJsonObject& obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

} // namespace

QString normalizeModelId(QStringView id)
{
    const QString s = id.toString();
    if (s == kModelSapi) {
        return s;
    }
    if (isLegacyEleven(s)) {
        return kModelTurbo.toString();
    }
    return kModelSapi.toString();
}

bool isElevenModel(QStringView id)
{
    return normalizeModelId(id) == kModelTurbo;
}

bool phraseHasInlineTags(QStringView text)
{
    return tagRe().matchView(text).hasMatch();
}

QString stripInlineTags(QStringView text)
{
    QString s(text);
    s.replace(tagRe(), QStringLiteral(" "));
    return s.simplified();
}

bool hasNonTagSpeechContent(QStringView text)
{
    return !stripInlineTags(text).isEmpty();
}

QString normalizeStandingTag(QStringView raw)
{
    QString t = raw.toString().trimmed();
    if (t.size() >= 2 && t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']'))) {
        t = t.mid(1, t.size() - 2).trimmed();
    }
    t.remove(QLatin1Char('['));
    t.remove(QLatin1Char(']'));
    return t.simplified();
}

QString applyStandingTag(QStringView phrase, QStringView tag)
{
    const QString clean = normalizeStandingTag(tag);
    const QString text = phrase.toString();
    if (clean.isEmpty()) {
        return text;
    }
    const QString wrapped = QLatin1Char('[') + clean + QLatin1Char(']');
    int lead = 0;
    while (lead < text.size() && text.at(lead).isSpace()) {
        ++lead;
    }
    const QStringView head = QStringView(text).mid(lead);
    if (head.size() >= wrapped.size()
        && head.left(wrapped.size()).compare(QStringView(wrapped), Qt::CaseInsensitive) == 0) {
        return text;
    }
    return wrapped + QLatin1Char(' ') + text;
}

namespace {

bool isDigitChar(QChar ch)
{
    return ch >= QLatin1Char('0') && ch <= QLatin1Char('9');
}

bool isAsciiLetter(QChar ch)
{
    return (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
           || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'));
}

bool isClosingMark(QChar ch)
{
    return ch == QLatin1Char('"') || ch == QLatin1Char('\'') || ch == QLatin1Char(')')
           || ch == QLatin1Char(']') || ch == QChar(0x201D) || ch == QChar(0x2019);
}

bool isBreathPunct(const QString& text, int index)
{
    const QChar ch = text.at(index);
    if (ch == QLatin1Char('!') || ch == QLatin1Char('?') || ch == QChar(0x2026)) {
        return true;
    }
    if (ch != QLatin1Char('.') && ch != QLatin1Char(',') && ch != QLatin1Char(';')
        && ch != QLatin1Char(':')) {
        return false;
    }
    const QChar prev = index > 0 ? text.at(index - 1) : QChar();
    const QChar next = index + 1 < text.size() ? text.at(index + 1) : QChar();
    if (isDigitChar(prev) && isDigitChar(next)) {
        return false;
    }
    if (ch == QLatin1Char('.') && isAsciiLetter(prev) && isAsciiLetter(next)) {
        return false;
    }
    return true;
}

QStringList splitAtPunctuation(const QString& src)
{
    QStringList parts;
    QString buf;
    int depth = 0;
    const auto push = [&]() {
        const QString piece = buf.trimmed();
        buf.clear();
        if (!piece.isEmpty()) {
            parts.push_back(piece);
        }
    };
    for (int i = 0; i < src.size(); ++i) {
        const QChar ch = src.at(i);
        if (ch == QLatin1Char('[')) {
            ++depth;
        }
        if (depth > 0) {
            buf += ch;
            if (ch == QLatin1Char(']')) {
                --depth;
            }
            continue;
        }
        if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            push();
            if (ch == QLatin1Char('\r') && i + 1 < src.size() && src.at(i + 1) == QLatin1Char('\n')) {
                ++i;
            }
            continue;
        }
        if (ch == QChar(0x2014) || ch == QChar(0x2013)
            || (ch == QLatin1Char('-') && i + 1 < src.size() && src.at(i + 1) == QLatin1Char('-'))) {
            buf += ch == QLatin1Char('-') ? QStringLiteral("--") : QString(ch);
            if (ch == QLatin1Char('-')) {
                ++i;
            }
            push();
            continue;
        }
        if (isBreathPunct(src, i)) {
            buf += ch;
            const QString marks = QStringLiteral("!?.") + QChar(0x2026) + QStringLiteral(",;:");
            while (i + 1 < src.size() && marks.contains(src.at(i + 1))) {
                ++i;
                buf += src.at(i);
            }
            while (i + 1 < src.size() && isClosingMark(src.at(i + 1))) {
                ++i;
                buf += src.at(i);
            }
            push();
            continue;
        }
        buf += ch;
    }
    push();
    return parts;
}

QString gluePieces(const QString& left, const QString& right)
{
    if (!left.isEmpty() && !right.isEmpty()) {
        return (left + QLatin1Char(' ') + right).trimmed();
    }
    return (left.isEmpty() ? right : left).trimmed();
}

QStringList breathParts(const QString& text)
{
    const QStringList raw = splitAtPunctuation(text);
    QStringList parts;
    QString held;
    for (const QString& piece : raw) {
        const QString combined = gluePieces(held, piece);
        held.clear();
        if (!hasNonTagSpeechContent(combined)) {
            held = combined;
            continue;
        }
        parts.push_back(combined);
    }
    if (!held.isEmpty()) {
        if (!parts.isEmpty()) {
            parts.last() = gluePieces(parts.last(), held);
        } else {
            parts.push_back(held);
        }
    }
    return parts;
}

QString leadingTagRun(const QString& text)
{
    static const QRegularExpression re(QStringLiteral(R"(^\s*((?:\[[^\]]*\]\s*)+))"));
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return {};
    }
    return match.captured(1).simplified();
}

} // namespace

QStringList taggedSends(QStringView phrase, QStringView tag)
{
    const QString text = phrase.toString();
    const QString clean = normalizeStandingTag(tag);
    if (clean.isEmpty() && !phraseHasInlineTags(text)) {
        return {text};
    }
    const QStringList parts = breathParts(text);
    if (parts.size() <= 1) {
        return {applyStandingTag(text, clean)};
    }
    const QString lead = leadingTagRun(text);
    QStringList sends;
    for (QString body : parts) {
        body = body.trimmed();
        if (!lead.isEmpty() && (body.isEmpty() || body.front() != QLatin1Char('['))) {
            body = (lead + QLatin1Char(' ') + body).trimmed();
        }
        if (!hasNonTagSpeechContent(body)) {
            continue;
        }
        sends.push_back(applyStandingTag(body, clean));
    }
    if (sends.isEmpty()) {
        return {applyStandingTag(text, clean)};
    }
    return sends;
}

QString keyHandoffUrl(QStringView challenge, QStringView state)
{
    QUrl url(QStringLiteral("https://elevenlabs.io/app/oauth/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"),
                       QStringLiteral("c9c18126-5718-406a-8a12-21b69e5a888d"));
    query.addQueryItem(QStringLiteral("redirect_uri"),
                       QStringLiteral("http://localhost:8484/callback"));
    query.addQueryItem(QStringLiteral("scope"),
                       QStringLiteral("text_to_speech voices_read user_read"));
    query.addQueryItem(QStringLiteral("state"), state.toString());
    query.addQueryItem(QStringLiteral("code_challenge"), challenge.toString());
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("key_handoff"), QStringLiteral("clipboard"));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QString keyQuickstartUrl()
{
    return QStringLiteral("https://elevenlabs.io/app/api/api-keys?quickstart=true");
}

Prepared prepareSpeakRequest(QStringView phrase, QStringView selectedModel, double speed)
{
    const QString phraseStr(phrase);
    const QString modelId = normalizeModelId(selectedModel);
    Prepared out;
    out.modelId = modelId;
    out.text = modelId == kModelTurbo ? phraseStr : stripInlineTags(phraseStr);
    out.localSpeed = clampOrDefault(speed, kDesiredSpeedMin, kDesiredSpeedMax, 1.0);
    return out;
}

QString dialogueStreamUrl()
{
    QUrl url(QStringLiteral("wss://api.elevenlabs.io/v1/text-to-dialogue/stream-input"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("model_id"), kModelTurbo.toString());
    query.addQueryItem(QStringLiteral("output_format"),
                       QStringLiteral("pcm_%1").arg(PcmWav::kSampleRate));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QByteArray registerVoicesMessage(QStringView voiceId)
{
    QJsonObject obj;
    QJsonArray voices;
    voices.append(voiceId.toString());
    obj.insert(QStringLiteral("voices"), voices);
    return jsonBytes(obj);
}

QByteArray speakInputsMessage(QStringView text, QStringView voiceId)
{
    QJsonObject line;
    line.insert(QStringLiteral("text"), text.toString());
    line.insert(QStringLiteral("voice_id"), voiceId.toString());
    line.insert(QStringLiteral("new_turn"), true);
    QJsonArray inputs;
    inputs.append(line);
    QJsonObject obj;
    obj.insert(QStringLiteral("inputs"), inputs);
    return jsonBytes(obj);
}

QByteArray closeSocketMessage()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("close_socket"), true);
    return jsonBytes(obj);
}

QByteArray keepAliveMessage()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("keep_alive"), true);
    return jsonBytes(obj);
}

} // namespace ElevenRequest
} // namespace gazer

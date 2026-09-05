#include "assist/SpeechSecrets.h"

#include "utils/AtomicFile.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace gazer {

QString SpeechSecrets::filePath()
{
    const QString dir =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(
            QStringLiteral("secrets"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("eleven.dpapi"));
}

#ifdef Q_OS_WIN

namespace {

QByteArray protect(const QString& plain, QString* error)
{
    const QByteArray utf8 = plain.toUtf8();
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.constData()));
    in.cbData = DWORD(utf8.size());
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Gazer ElevenLabs", nullptr, nullptr, nullptr, 0, &out)) {
        if (error) {
            *error = QStringLiteral("DPAPI protect failed");
        }
        return {};
    }
    QByteArray blob(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    return blob;
}

QString unprotect(const QByteArray& blob, QString* error)
{
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()));
    in.cbData = DWORD(blob.size());
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        if (error) {
            *error = QStringLiteral("DPAPI unprotect failed");
        }
        return {};
    }
    const QString plain = QString::fromUtf8(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return plain;
}

} // namespace

#endif

bool SpeechSecrets::store(const QString& apiKey, QString* error)
{
    const QString key = apiKey.trimmed();
    if (key.isEmpty()) {
        return clear(error);
    }
#ifdef Q_OS_WIN
    const QByteArray blob = protect(key, error);
    if (blob.isEmpty()) {
        return false;
    }
    if (!writeFileAtomically(filePath(), blob, error)) {
        return false;
    }
    m_cached = true;
    m_has = true;
    m_plain = key;
    GAZER_INFO << "ElevenLabs key stored (DPAPI)";
    return true;
#else
    Q_UNUSED(error);
    GAZER_WARN << "SpeechSecrets: DPAPI not available";
    return false;
#endif
}

bool SpeechSecrets::loadIntoCache(QString* error) const
{
    if (m_cached) {
        return m_has;
    }
#ifdef Q_OS_WIN
    QFile f(filePath());
    if (!f.exists()) {
        m_cached = true;
        m_has = false;
        m_plain.clear();
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot read key file");
        }
        return false;
    }
    const QString plain = unprotect(f.readAll(), error);
    m_cached = true;
    m_has = !plain.isEmpty();
    m_plain = plain;
    return m_has;
#else
    Q_UNUSED(error);
    m_cached = true;
    m_has = false;
    return false;
#endif
}

bool SpeechSecrets::load(QString* apiKey, QString* error)
{
    const bool ok = loadIntoCache(error);
    if (apiKey) {
        *apiKey = m_plain;
    }
    return ok;
}

bool SpeechSecrets::clear(QString* error)
{
    forgetCache();
    const QString path = filePath();
    if (QFile::exists(path) && !QFile::remove(path)) {
        if (error) {
            *error = QStringLiteral("Cannot delete key file");
        }
        return false;
    }
    GAZER_INFO << "ElevenLabs key cleared";
    return true;
}

bool SpeechSecrets::hasKey() const
{
    return loadIntoCache();
}

QString SpeechSecrets::lastFour() const
{
    if (!loadIntoCache() || m_plain.size() < 4) {
        return {};
    }
    return m_plain.right(4);
}

void SpeechSecrets::forgetCache()
{
#ifdef Q_OS_WIN
    if (!m_plain.isEmpty()) {
        m_plain.fill(QLatin1Char('\0'));
    }
#endif
    m_plain.clear();
    m_has = false;
    m_cached = false;
}

} // namespace gazer

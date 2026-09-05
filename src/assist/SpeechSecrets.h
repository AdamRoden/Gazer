#pragma once

#include <QString>

namespace gazer {

/// ElevenLabs API key in DPAPI (`%AppData%/Gazer/secrets/eleven.dpapi`). Never log the key.
class SpeechSecrets {
public:
    [[nodiscard]] static QString filePath();

    [[nodiscard]] bool store(const QString& apiKey, QString* error = nullptr);
    [[nodiscard]] bool load(QString* apiKey, QString* error = nullptr);
    bool clear(QString* error = nullptr);
    [[nodiscard]] bool hasKey() const;
    [[nodiscard]] QString lastFour() const;
    void forgetCache();

private:
    bool loadIntoCache(QString* error = nullptr) const;

    mutable bool m_cached = false;
    mutable bool m_has = false;
    mutable QString m_plain;
};

} // namespace gazer

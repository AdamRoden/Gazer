#pragma once

#include <QObject>
#include <QString>

namespace gazer {

/// Text-to-speech. Windows: SAPI ISpVoice (async). Other platforms: log-only stub.
class TtsService final : public QObject {
    Q_OBJECT

public:
    explicit TtsService(QObject* parent = nullptr);
    ~TtsService() override;

    [[nodiscard]] bool isAvailable() const { return m_available; }
    [[nodiscard]] bool speak(const QString& text, QString* error = nullptr);
    void stop();

signals:
    void started(const QString& text);
    void failed(const QString& error);

private:
    bool m_available = false;
    void* m_voice = nullptr; // ISpVoice* on Windows
};

} // namespace gazer

#pragma once

#include "assist/TtsService.h"
#include "input/InputService.h"
#include "mapping/MappingEngine.h"

#include <QObject>
#include <QString>

namespace gazer {

/// Single path for speak: TTS + optional type-through from mapping profile.
class PhraseService final : public QObject {
    Q_OBJECT

public:
    PhraseService(TtsService& tts, InputService& input, MappingEngine& mapping,
                  QObject* parent = nullptr);

    /// Voice (if available) and optional keyboard type-through.
    [[nodiscard]] bool speak(const QString& text, QString* error = nullptr);

signals:
    void spoken(const QString& text);
    void failed(const QString& error);

private:
    TtsService& m_tts;
    InputService& m_input;
    MappingEngine& m_mapping;
};

} // namespace gazer

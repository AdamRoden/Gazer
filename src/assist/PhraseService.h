#pragma once

#include "assist/SpeechEngine.h"
#include "input/InputService.h"
#include "mapping/MappingEngine.h"

#include <QObject>
#include <QString>

namespace gazer {

/// Single path for speak: SpeechEngine + optional type-through from mapping profile.
class PhraseService final : public QObject {
    Q_OBJECT

public:
    PhraseService(SpeechEngine& engine, InputService& input, MappingEngine& mapping,
                  QObject* parent = nullptr);

    /// Canned overload; XML `<Speak>` and `gazer.speak`.
    [[nodiscard]] bool speak(const QString& text, QString* error = nullptr);
    /// Composer Speak and soundboard live utterance. `recordHistory` is composer Speak only.
    [[nodiscard]] bool speak(const QString& text, SpeakKind kind, QString* error = nullptr,
                             bool recordHistory = false);
    /// Baked MPEG. Types `sourceText` when speakAlsoType is on.
    [[nodiscard]] bool playClip(const QString& path, const QString& sourceText,
                                QString* error = nullptr);

signals:
    void spoken(const QString& text);
    void failed(const QString& error);

private:
    SpeechEngine& m_engine;
    InputService& m_input;
    MappingEngine& m_mapping;
};

} // namespace gazer

#include "assist/PhraseService.h"

#include "assist/ElevenRequest.h"
#include "input/InputTypes.h"
#include "utils/Log.h"

namespace gazer {

PhraseService::PhraseService(SpeechEngine& engine, InputService& input, MappingEngine& mapping,
                             QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_input(input)
    , m_mapping(mapping)
{
    connect(&m_engine, &SpeechEngine::failed, this, &PhraseService::failed);
}

bool PhraseService::speak(const QString& text, QString* error)
{
    return speak(text, SpeakKind::Canned, error);
}

bool PhraseService::speak(const QString& text, SpeakKind kind, QString* error, bool recordHistory)
{
    if (text.isEmpty()) {
        return true;
    }

    m_engine.speak(text, kind, recordHistory);

    // Composer Speak is voice output. Type-through stays on canned XML <Speak>.
    if (kind != SpeakKind::Composed && m_mapping.speakAlsoType()) {
        InputOutput o;
        o.type = InputOutput::Type::Text;
        o.value = ElevenRequest::stripInlineTags(text);
        QString err;
        if (!o.value.isEmpty() && !m_input.execute(o, &err)) {
            if (error) {
                *error = err;
            }
            emit failed(err);
            return false;
        }
    }

    emit spoken(text);
    return true;
}

bool PhraseService::playClip(const QString& path, const QString& sourceText, QString* error)
{
    if (!m_engine.playFile(path)) {
        if (error) {
            *error = QStringLiteral("Clip play failed");
        }
        return false;
    }
    if (m_mapping.speakAlsoType()) {
        InputOutput o;
        o.type = InputOutput::Type::Text;
        o.value = ElevenRequest::stripInlineTags(sourceText);
        QString err;
        if (!o.value.isEmpty() && !m_input.execute(o, &err)) {
            if (error) {
                *error = err;
            }
            emit failed(err);
            return false;
        }
    }
    return true;
}

} // namespace gazer

#include "assist/PhraseService.h"

#include "input/InputTypes.h"
#include "utils/Log.h"

namespace gazer {

PhraseService::PhraseService(TtsService& tts, InputService& input, MappingEngine& mapping,
                             QObject* parent)
    : QObject(parent)
    , m_tts(tts)
    , m_input(input)
    , m_mapping(mapping)
{
}

bool PhraseService::speak(const QString& text, QString* error)
{
    if (text.isEmpty()) {
        return true;
    }

    QString err;
    if (m_tts.isAvailable()) {
        if (!m_tts.speak(text, &err)) {
            if (error) {
                *error = err;
            }
            emit failed(err);
            // Still attempt type-through.
        }
    }

    if (m_mapping.speakAlsoType()) {
        InputOutput o;
        o.type = InputOutput::Type::Text;
        o.value = text;
        if (!m_input.execute(o, &err)) {
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

} // namespace gazer

#include "assist/ScriptHost.h"

#include "assist/SpeechEngine.h"
#include "input/InputTypes.h"
#include "utils/Log.h"

#include <QJSValue>

namespace gazer {

ScriptApi::ScriptApi(SpeechEngine& speech, CommandRegistry& commands, InputService& input,
                     PageSession& pages, QObject* parent)
    : QObject(parent)
    , m_speech(speech)
    , m_commands(commands)
    , m_input(input)
    , m_pages(pages)
{
}

void ScriptApi::log(const QString& message)
{
    GAZER_INFO << "[script]" << message;
    emit statusMessage(message);
}

void ScriptApi::speak(const QString& text)
{
    m_speech.speak(text, SpeakKind::Canned);
    emit statusMessage(QStringLiteral("Said: %1").arg(text));
}

void ScriptApi::typeText(const QString& text)
{
    InputOutput o;
    o.type = InputOutput::Type::Text;
    o.value = text;
    QString err;
    if (!m_input.execute(o, &err)) {
        emit statusMessage(err);
    }
}

bool ScriptApi::runCommand(const QString& name)
{
    QString err;
    const bool ok = m_commands.run(name, &err);
    if (!ok) {
        emit statusMessage(err);
    }
    return ok;
}

bool ScriptApi::openPage(const QString& pageId)
{
    QString err;
    return m_pages.openPage(pageId, &err);
}

bool ScriptApi::loadPage(const QString& pageId)
{
    const QString cur = m_pages.topPageId();
    if (!cur.isEmpty() && cur != m_pages.root().id) {
        m_pages.closePage(cur);
    }
    QString err;
    return m_pages.openPage(pageId, &err);
}

QString ScriptApi::focusedPageId() const
{
    return m_pages.topPageId();
}

ScriptHost::ScriptHost(SpeechEngine& speech, CommandRegistry& commands, InputService& input,
                       PageSession& pages, QObject* parent)
    : QObject(parent)
{
    m_api = new ScriptApi(speech, commands, input, pages, this);
    connect(m_api, &ScriptApi::statusMessage, this, &ScriptHost::statusMessage);

    const QJSValue gazerObj = m_engine.newQObject(m_api);
    m_engine.globalObject().setProperty(QStringLiteral("gazer"), gazerObj);
}

bool ScriptHost::evaluate(const QString& source, QString* error)
{
    if (source.trimmed().isEmpty()) {
        return true;
    }
    const QJSValue result = m_engine.evaluate(source, QStringLiteral("page-script"));
    if (result.isError()) {
        const QString msg = QStringLiteral("%1:%2: %3")
                                .arg(result.property(QStringLiteral("fileName")).toString())
                                .arg(result.property(QStringLiteral("lineNumber")).toInt())
                                .arg(result.toString());
        GAZER_WARN << "Script error:" << msg;
        if (error) {
            *error = msg;
        }
        return false;
    }
    return true;
}

} // namespace gazer

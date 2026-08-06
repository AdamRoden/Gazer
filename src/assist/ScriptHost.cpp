#include "assist/ScriptHost.h"

#include "input/InputTypes.h"
#include "layout/SessionNavigate.h"
#include "utils/Log.h"

#include <QJSValue>

namespace gazer {

ScriptApi::ScriptApi(PhraseService& phrases, CommandRegistry& commands, InputService& input,
                     LayoutInstanceManager& instances, QObject* parent)
    : QObject(parent)
    , m_phrases(phrases)
    , m_commands(commands)
    , m_input(input)
    , m_instances(instances)
{
}

void ScriptApi::log(const QString& message)
{
    GAZER_INFO << "[script]" << message;
    emit statusMessage(message);
}

void ScriptApi::speak(const QString& text)
{
    QString err;
    if (!m_phrases.speak(text, &err) && !err.isEmpty()) {
        emit statusMessage(err);
        return;
    }
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

bool ScriptApi::openLayout(const QString& layoutId)
{
    QString err;
    return !m_instances.openInstance(layoutId, &err).isEmpty();
}

bool ScriptApi::loadLayout(const QString& layoutId)
{
    auto* focused = m_instances.focusedInstance();
    if (!focused) {
        return false;
    }
    QString err;
    return applyLoadLayout(m_instances, focused->instanceId(), layoutId, &err);
}

QString ScriptApi::focusedLayoutId() const
{
    if (!m_instances.focusedInstance()) {
        return {};
    }
    return m_instances.focusedInstance()->layoutId();
}

ScriptHost::ScriptHost(PhraseService& phrases, CommandRegistry& commands, InputService& input,
                       LayoutInstanceManager& instances, QObject* parent)
    : QObject(parent)
{
    m_api = new ScriptApi(phrases, commands, input, instances, this);
    connect(m_api, &ScriptApi::statusMessage, this, &ScriptHost::statusMessage);

    const QJSValue gazerObj = m_engine.newQObject(m_api);
    m_engine.globalObject().setProperty(QStringLiteral("gazer"), gazerObj);
}

bool ScriptHost::evaluate(const QString& source, QString* error)
{
    if (source.trimmed().isEmpty()) {
        return true;
    }
    const QJSValue result = m_engine.evaluate(source, QStringLiteral("layout-script"));
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

#pragma once

#include "app/CommandRegistry.h"
#include "assist/PhraseService.h"
#include "input/InputService.h"
#include "layout/LayoutInstanceManager.h"

#include <QJSEngine>
#include <QObject>
#include <QString>

namespace gazer {

/// Privileged script API exposed as global `gazer` (not a security sandbox).
class ScriptApi final : public QObject {
    Q_OBJECT

public:
    ScriptApi(PhraseService& phrases, CommandRegistry& commands, InputService& input,
              LayoutInstanceManager& instances, QObject* parent = nullptr);

public slots:
    void log(const QString& message);
    void speak(const QString& text);
    void typeText(const QString& text);
    bool runCommand(const QString& name);
    bool openLayout(const QString& layoutId);
    bool loadLayout(const QString& layoutId);
    QString focusedLayoutId() const;

signals:
    void statusMessage(const QString& message);

private:
    PhraseService& m_phrases;
    CommandRegistry& m_commands;
    InputService& m_input;
    LayoutInstanceManager& m_instances;
};

class ScriptHost final : public QObject {
    Q_OBJECT

public:
    ScriptHost(PhraseService& phrases, CommandRegistry& commands, InputService& input,
               LayoutInstanceManager& instances, QObject* parent = nullptr);

    [[nodiscard]] bool evaluate(const QString& source, QString* error = nullptr);
    [[nodiscard]] ScriptApi* api() const { return m_api; }

signals:
    void statusMessage(const QString& message);

private:
    QJSEngine m_engine;
    ScriptApi* m_api = nullptr;
};

} // namespace gazer

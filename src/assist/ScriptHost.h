#pragma once

#include "app/CommandRegistry.h"
#include "assist/PhraseService.h"
#include "input/InputService.h"
#include "layout/PageSession.h"

#include <QJSEngine>
#include <QObject>
#include <QString>

namespace gazer {

/// Privileged script API exposed as global `gazer` (not a security sandbox).
class ScriptApi final : public QObject {
    Q_OBJECT

public:
    ScriptApi(PhraseService& phrases, CommandRegistry& commands, InputService& input,
              PageSession& pages, QObject* parent = nullptr);

public slots:
    void log(const QString& message);
    void speak(const QString& text);
    void typeText(const QString& text);
    bool runCommand(const QString& name);
    bool openPage(const QString& pageId);
    bool loadPage(const QString& pageId);
    QString focusedPageId() const;

signals:
    void statusMessage(const QString& message);

private:
    PhraseService& m_phrases;
    CommandRegistry& m_commands;
    InputService& m_input;
    PageSession& m_pages;
};

class ScriptHost final : public QObject {
    Q_OBJECT

public:
    ScriptHost(PhraseService& phrases, CommandRegistry& commands, InputService& input,
               PageSession& pages, QObject* parent = nullptr);

    [[nodiscard]] bool evaluate(const QString& source, QString* error = nullptr);
    [[nodiscard]] ScriptApi* api() const { return m_api; }

signals:
    void statusMessage(const QString& message);

private:
    QJSEngine m_engine;
    ScriptApi* m_api = nullptr;
};

} // namespace gazer

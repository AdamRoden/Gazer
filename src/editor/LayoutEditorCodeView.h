#pragma once

#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QPlainTextEdit>

namespace gazer {

class LayoutEditorSession;
class LayoutEditorXmlHighlight;

/// XML source for the current page. Apply parses back into the session.
class LayoutEditorCodeView final : public QPlainTextEdit {
public:
    explicit LayoutEditorCodeView(QWidget* parent = nullptr);

    void setTheme(const ThemeColors& theme);
    void loadDocument(const PageDocument& doc);
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    [[nodiscard]] bool applyTo(LayoutEditorSession& session, QWidget* errorParent);

private:
    LayoutEditorXmlHighlight* m_highlight = nullptr;
    bool m_dirty = false;
};

} // namespace gazer

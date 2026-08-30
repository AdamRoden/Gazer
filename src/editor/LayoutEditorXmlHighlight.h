#pragma once

#include "ui/Theme.h"

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace gazer {

class LayoutEditorXmlHighlight final : public QSyntaxHighlighter {
public:
    explicit LayoutEditorXmlHighlight(QTextDocument* parent = nullptr);
    void setTheme(const ThemeColors& theme);

protected:
    void highlightBlock(const QString& text) override;

private:
    QTextCharFormat m_tag;
    QTextCharFormat m_attr;
    QTextCharFormat m_value;
    QTextCharFormat m_comment;
    QTextCharFormat m_punct;
};

} // namespace gazer

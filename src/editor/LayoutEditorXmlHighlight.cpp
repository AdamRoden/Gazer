#include "editor/LayoutEditorXmlHighlight.h"

#include <QFont>

namespace gazer {
namespace {

QTextCharFormat fmt(const QColor& c, bool bold = false)
{
    QTextCharFormat f;
    f.setForeground(c);
    if (bold) {
        f.setFontWeight(QFont::DemiBold);
    }
    return f;
}

} // namespace

LayoutEditorXmlHighlight::LayoutEditorXmlHighlight(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setTheme(ThemeColors::darkPreset());
}

void LayoutEditorXmlHighlight::setTheme(const ThemeColors& theme)
{
    m_tag = fmt(theme.accent, true);
    m_attr = fmt(theme.text);
    QColor value = theme.accentHover.isValid() ? theme.accentHover : theme.accent;
    m_value = fmt(value);
    m_comment = fmt(theme.textSecondary);
    m_punct = fmt(theme.textSecondary);
    rehighlight();
}

void LayoutEditorXmlHighlight::highlightBlock(const QString& text)
{
    enum State { Normal = 0, Comment = 1, Tag = 2 };
    int state = previousBlockState();
    if (state < 0) {
        state = Normal;
    }
    int i = 0;
    const int n = text.size();
    auto isName = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char(':')
               || c == QLatin1Char('-');
    };
    while (i < n) {
        if (state == Comment) {
            const int end = text.indexOf(QStringLiteral("-->"), i);
            if (end < 0) {
                setFormat(i, n - i, m_comment);
                setCurrentBlockState(Comment);
                return;
            }
            setFormat(i, end + 3 - i, m_comment);
            i = end + 3;
            state = Normal;
            continue;
        }
        if (text.mid(i, 4) == QLatin1String("<!--")) {
            setFormat(i, 4, m_comment);
            i += 4;
            state = Comment;
            continue;
        }
        if (state == Normal && text[i] == QLatin1Char('<')) {
            setFormat(i, 1, m_punct);
            ++i;
            if (i < n && (text[i] == QLatin1Char('/') || text[i] == QLatin1Char('?'))) {
                setFormat(i, 1, m_punct);
                ++i;
            }
            const int start = i;
            while (i < n && isName(text[i])) {
                ++i;
            }
            if (i > start) {
                setFormat(start, i - start, m_tag);
            }
            state = Tag;
            continue;
        }
        if (state == Tag) {
            if (text[i] == QLatin1Char('>') || (text[i] == QLatin1Char('/') && i + 1 < n
                                                && text[i + 1] == QLatin1Char('>'))) {
                const int len = text[i] == QLatin1Char('/') ? 2 : 1;
                setFormat(i, len, m_punct);
                i += len;
                state = Normal;
                continue;
            }
            if (text[i] == QLatin1Char('=')) {
                setFormat(i, 1, m_punct);
                ++i;
                continue;
            }
            if (text[i] == QLatin1Char('"') || text[i] == QLatin1Char('\'')) {
                const QChar q = text[i];
                const int start = i;
                ++i;
                while (i < n && text[i] != q) {
                    ++i;
                }
                if (i < n) {
                    ++i;
                }
                setFormat(start, i - start, m_value);
                continue;
            }
            if (isName(text[i])) {
                const int start = i;
                while (i < n && isName(text[i])) {
                    ++i;
                }
                setFormat(start, i - start, m_attr);
                continue;
            }
            ++i;
            continue;
        }
        ++i;
    }
    setCurrentBlockState(state);
}

} // namespace gazer

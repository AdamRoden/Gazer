#include "editor/LayoutEditorCodeView.h"

#include "editor/LayoutEditorSession.h"
#include "editor/LayoutEditorXmlHighlight.h"
#include "layout/PageLoader.h"
#include "layout/PageWriter.h"

#include <QFont>
#include <QMessageBox>
#include <QSignalBlocker>

namespace gazer {

LayoutEditorCodeView::LayoutEditorCodeView(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("codeView"));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(24);
    setFont(QFont(QStringLiteral("Cascadia Mono"), 11));
    if (font().family() != QLatin1String("Cascadia Mono")) {
        setFont(QFont(QStringLiteral("Consolas"), 11));
    }
    m_highlight = new LayoutEditorXmlHighlight(document());
    connect(this, &QPlainTextEdit::textChanged, this, [this]() { m_dirty = true; });
}

void LayoutEditorCodeView::setTheme(const ThemeColors& theme)
{
    if (m_highlight) {
        m_highlight->setTheme(theme);
    }
}

void LayoutEditorCodeView::loadDocument(const PageDocument& doc)
{
    const QSignalBlocker block(this);
    setPlainText(QString::fromUtf8(PageWriter::toBytes(doc)));
    m_dirty = false;
}

bool LayoutEditorCodeView::applyTo(LayoutEditorSession& session, QWidget* errorParent)
{
    if (!m_dirty) {
        return true;
    }
    PageDocument doc;
    QString err;
    if (!PageLoader::loadFromXml(toPlainText().toUtf8(), doc, &err)) {
        QMessageBox::warning(errorParent, QStringLiteral("Invalid page XML"), err);
        return false;
    }
    m_dirty = false;
    session.edit(QStringLiteral("Edit XML"), [&](PageDocument& d) { d = std::move(doc); });
    return true;
}

} // namespace gazer

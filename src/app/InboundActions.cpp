#include "app/InboundActions.h"

#include "layout/PageActionParse.h"

#include <QXmlStreamReader>

namespace gazer {

namespace {

constexpr auto kRaise = "raise";

QString stripBom(QString s)
{
    if (s.startsWith(QChar(0xFEFF))) {
        s.remove(0, 1);
    }
    return s;
}

bool looksLikeXml(const QString& t)
{
    return t.startsWith(QLatin1Char('<'));
}

bool parseXmlActions(const QString& fragment, QVector<PageAction>& out, QString* error)
{
    QString body = fragment.trimmed();
    if (body.startsWith(QLatin1String("<?xml"), Qt::CaseInsensitive)) {
        const int end = body.indexOf(QLatin1Char('>'));
        if (end >= 0) {
            body = body.mid(end + 1).trimmed();
        }
    }
    QXmlStreamReader xml(QByteArrayLiteral("<Actions>") + body.toUtf8()
                         + QByteArrayLiteral("</Actions>"));
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("Actions")) {
        if (error) {
            *error = QStringLiteral("Invalid inbound XML");
        }
        return false;
    }
    while (xml.readNextStartElement()) {
        const QString name = xml.name().toString();
        if (!isPageActionElementName(name)) {
            if (error) {
                *error = QStringLiteral("Unknown action '%1'").arg(name);
            }
            return false;
        }
        const QXmlStreamAttributes attrs = xml.attributes();
        const QString text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
        PageAction act;
        if (!parsePageActionElement(name, attrs, text, act, error)) {
            return false;
        }
        out.push_back(act);
    }
    if (xml.hasError()) {
        if (error) {
            *error = xml.errorString();
        }
        return false;
    }
    return true;
}

bool parseLineAction(const QString& line, PageAction& out, QString* error)
{
    const int eq = line.indexOf(QLatin1Char('='));
    int sp = line.indexOf(QLatin1Char(' '));
    if (sp < 0) {
        sp = line.indexOf(QLatin1Char('\t'));
    }
    QString name;
    QString value;
    if (eq > 0 && (sp < 0 || eq < sp)) {
        name = line.left(eq).trimmed();
        value = line.mid(eq + 1);
    } else if (sp > 0) {
        name = line.left(sp).trimmed();
        value = line.mid(sp + 1).trimmed();
    } else {
        name = line.trimmed();
    }
    if (name.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty action name");
        }
        return false;
    }
    return parsePageActionAttribute(name, value, out, error);
}

} // namespace

QString inboundPayloadFromArgs(const QStringList& args)
{
    QStringList parts;
    for (int i = 1; i < args.size(); ++i) {
        const QString& a = args[i];
        if (a == QLatin1String("--action")) {
            if (i + 1 < args.size()) {
                parts.push_back(args[++i]);
            }
            continue;
        }
        if (a.startsWith(QLatin1String("--action="))) {
            parts.push_back(a.mid(QStringLiteral("--action=").size()));
        }
    }
    return parts.join(QLatin1Char('\n'));
}

QString inboundForwardPayload(const QStringList& args)
{
    QStringList parts;
    const QString actions = inboundPayloadFromArgs(args);
    if (!actions.isEmpty()) {
        parts.push_back(actions);
    }
    if (args.contains(QLatin1String("--editor"))) {
        parts.push_back(QStringLiteral("command=openPageEditor"));
    }
    if (parts.isEmpty()) {
        return QString::fromLatin1(kRaise);
    }
    return parts.join(QLatin1Char('\n'));
}

bool isInboundRaise(const QString& text)
{
    return stripBom(text).trimmed().compare(QLatin1String(kRaise), Qt::CaseInsensitive) == 0;
}

bool parseInboundActions(const QString& text, QVector<PageAction>& out, QString* error)
{
    out.clear();
    const QString t = stripBom(text).trimmed();
    if (t.isEmpty() || isInboundRaise(t)) {
        return true;
    }
    if (looksLikeXml(t)) {
        return parseXmlActions(t, out, error);
    }

    const QStringList lines = t.split(QLatin1Char('\n'));
    for (QString raw : lines) {
        if (raw.endsWith(QLatin1Char('\r'))) {
            raw.chop(1);
        }
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (looksLikeXml(line)) {
            if (!parseXmlActions(line, out, error)) {
                return false;
            }
            continue;
        }
        PageAction a;
        if (!parseLineAction(line, a, error)) {
            return false;
        }
        out.push_back(a);
    }
    return true;
}

} // namespace gazer

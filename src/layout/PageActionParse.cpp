#include "layout/PageActionParse.h"

#include "layout/PageDim.h"

#include <QPair>
#include <QStringList>
#include <QtGlobal>
#include <QVector>

namespace gazer {

namespace {

struct ActionName {
    const char* attr;
    const char* element;
    PageActionType type;
    PageVerb verb;
    PageTargetKind kind;
};

const ActionName kNames[] = {
    {"send", "Send", PageActionType::Send, PageVerb::Open, PageTargetKind::Page},
    {"click", "Click", PageActionType::Click, PageVerb::Open, PageTargetKind::Page},
    {"move", "Move", PageActionType::Move, PageVerb::Open, PageTargetKind::Page},
    {"moveAndClick", "MoveAndClick", PageActionType::MoveAndClick, PageVerb::Open,
     PageTargetKind::Page},
    {"command", "Command", PageActionType::Command, PageVerb::Open, PageTargetKind::Page},
    {"openPage", "OpenPage", PageActionType::Nav, PageVerb::Open, PageTargetKind::Page},
    {"openGrid", "OpenGrid", PageActionType::Nav, PageVerb::Open, PageTargetKind::Grid},
    {"openZone", "OpenZone", PageActionType::Nav, PageVerb::Open, PageTargetKind::Zone},
    {"closePage", "ClosePage", PageActionType::Nav, PageVerb::Close, PageTargetKind::Page},
    {"closeGrid", "CloseGrid", PageActionType::Nav, PageVerb::Close, PageTargetKind::Grid},
    {"closeZone", "CloseZone", PageActionType::Nav, PageVerb::Close, PageTargetKind::Zone},
    {"togglePage", "TogglePage", PageActionType::Nav, PageVerb::Toggle, PageTargetKind::Page},
    {"toggleGrid", "ToggleGrid", PageActionType::Nav, PageVerb::Toggle, PageTargetKind::Grid},
    {"toggleZone", "ToggleZone", PageActionType::Nav, PageVerb::Toggle, PageTargetKind::Zone},
    {"goBack", "GoBack", PageActionType::GoBack, PageVerb::Open, PageTargetKind::Page},
    {"speak", "Speak", PageActionType::Speak, PageVerb::Open, PageTargetKind::Page},
};

const ActionName* findName(QStringView raw)
{
    const QString n = raw.toString().trimmed().toLower();
    if (n.isEmpty() || n == QLatin1String("action") || n == QLatin1String("page")) {
        return nullptr;
    }
    for (const ActionName& a : kNames) {
        if (n == QLatin1String(a.attr) || n == QString::fromLatin1(a.element).toLower()) {
            return &a;
        }
    }
    return nullptr;
}

const ActionName* findName(const PageAction& a)
{
    for (const ActionName& n : kNames) {
        if (n.type != a.type) {
            continue;
        }
        if (a.type == PageActionType::Nav
            && (n.verb != a.verb || n.kind != a.targetKind)) {
            continue;
        }
        return &n;
    }
    return nullptr;
}

QStringList splitCsv(const QString& value)
{
    QStringList out;
    for (const QString& p : value.split(QLatin1Char(','))) {
        out.push_back(p.trimmed());
    }
    while (!out.isEmpty() && out.last().isEmpty()) {
        out.removeLast();
    }
    return out;
}

QString csvJoin(const QStringList& parts)
{
    int last = parts.size();
    while (last > 0 && parts.at(last - 1).isEmpty()) {
        --last;
    }
    QStringList out;
    out.reserve(last);
    for (int i = 0; i < last; ++i) {
        out.push_back(parts.at(i));
    }
    return out.join(QStringLiteral(", "));
}

bool isEdgeToken(const QString& t)
{
    const QString l = t.trimmed().toLower();
    return l == QLatin1String("down") || l == QLatin1String("up");
}

bool parseIntToken(const QString& t, int* out, QString* error, const char* what)
{
    const QString s = t.trimmed();
    if (s.isEmpty()) {
        return true;
    }
    bool ok = false;
    const int v = s.toInt(&ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("%1 must be an integer, got '%2'")
                         .arg(QString::fromLatin1(what), t);
        }
        return false;
    }
    *out = v;
    return true;
}

bool looksNumeric(const QString& t)
{
    const QString s = t.trimmed();
    if (s.isEmpty()) {
        return false;
    }
    bool ok = false;
    (void)s.toDouble(&ok);
    return ok || s.contains(QLatin1Char('/'));
}

void applySpell(PageAction& out, const ActionName& n)
{
    out.type = n.type;
    if (n.type == PageActionType::Nav) {
        out.verb = n.verb;
        out.targetKind = n.kind;
    }
}

bool parseNavTarget(const QString& tok, PageAction& out)
{
    const QString t = tok.trimmed();
    const QString l = t.toLower();
    if (t == QLatin1String("-all") || l == QLatin1String("all")) {
        out.targetScope = PageNavScope::All;
        out.targetId.clear();
        return true;
    }
    if (t == QLatin1String("-self") || l == QLatin1String("self")) {
        out.targetScope = PageNavScope::Self;
        out.targetId.clear();
        return true;
    }
    if (t == QLatin1String("-!self") || t == QLatin1String("!self")) {
        out.targetScope = PageNavScope::Others;
        out.targetId.clear();
        return true;
    }
    out.targetScope = PageNavScope::Id;
    out.targetId = t;
    return true;
}

QString navTargetText(const PageAction& a)
{
    switch (a.targetScope) {
    case PageNavScope::All:
        return QStringLiteral("-all");
    case PageNavScope::Self:
        return QStringLiteral("-self");
    case PageNavScope::Others:
        return QStringLiteral("-!self");
    case PageNavScope::Id:
        break;
    }
    return a.targetId;
}

bool parseSendParts(const QStringList& parts, PageAction& out, QString* error)
{
    out.type = PageActionType::Send;
    if (!parts.isEmpty()) {
        out.sendKey = parts[0];
    }
    int i = 1;
    if (i < parts.size() && isEdgeToken(parts[i])) {
        out.sendEdge = parts[i];
        ++i;
    } else if (i < parts.size() && parts[i].isEmpty()) {
        ++i;
    }
    if (i < parts.size()) {
        if (!parseIntToken(parts[i], &out.sendDurationMs, error, "Send duration")) {
            return false;
        }
        ++i;
    }
    if (i < parts.size()) {
        if (error) {
            *error = QStringLiteral("Unexpected extra Send field '%1'").arg(parts[i]);
        }
        return false;
    }
    return true;
}

bool parseClickParts(const QStringList& parts, PageAction& out, QString* error)
{
    out.type = PageActionType::Click;
    if (!parts.isEmpty()) {
        out.button = parts[0];
    }
    int i = 1;
    const int n = parts.size();
    if (i < n && isEdgeToken(parts[i])) {
    } else if (i < n) {
        if (!parseIntToken(parts[i], &out.clickCount, error, "Click count")) {
            return false;
        }
        ++i;
    }
    if (i < n && isEdgeToken(parts[i])) {
        out.clickEdge = parts[i];
        ++i;
    } else if (i < n && parts[i].isEmpty()) {
        ++i;
    }
    if (i < n) {
        if (!parseIntToken(parts[i], &out.speed, error, "Click speed")) {
            return false;
        }
        ++i;
    }
    if (i < n) {
        if (error) {
            *error = QStringLiteral("Unexpected extra field '%1'").arg(parts[i]);
        }
        return false;
    }
    return true;
}

bool parseZoomInt(const QString& tok, PageZoomMode omitted, PageAction& out, QString* error)
{
    if (tok.trimmed().isEmpty()) {
        out.zoomMode = omitted;
        return true;
    }
    int z = 0;
    if (!parseIntToken(tok, &z, error, "Zoom")) {
        return false;
    }
    if (z <= 0) {
        out.zoomMode = PageZoomMode::Off;
        out.zoomLevel = 0;
    } else {
        out.zoomMode = PageZoomMode::Level;
        out.zoomLevel = z;
    }
    return true;
}

bool parseMoveParts(const QStringList& parts, PageAction& out, QString* error)
{
    out.type = PageActionType::Move;
    if (parts.isEmpty()) {
        out.moveMode = PageMoveMode::Gaze;
        out.zoomMode = PageZoomMode::Settings;
        return true;
    }
    const QString m0 = parts[0].trimmed();
    const QString l0 = m0.toLower();

    if (l0 == QLatin1String("gaze")) {
        out.moveMode = PageMoveMode::Gaze;
        if (parts.size() >= 2) {
            if (!parseZoomInt(parts[1], PageZoomMode::Settings, out, error)) {
                return false;
            }
        } else {
            out.zoomMode = PageZoomMode::Settings;
        }
        if (parts.size() >= 3) {
            if (error) {
                *error = QStringLiteral("Unexpected extra Move field '%1'").arg(parts[2]);
            }
            return false;
        }
        return true;
    }

    if (l0 == QLatin1String("relative") || l0 == QLatin1String("absolute")) {
        out.moveMode =
            l0 == QLatin1String("relative") ? PageMoveMode::Relative : PageMoveMode::Absolute;
        if (parts.size() >= 2) {
            out.moveX = PageDimParse::parse(parts[1], error);
            if (error && !error->isEmpty()) {
                return false;
            }
        }
        if (parts.size() >= 3) {
            out.moveY = PageDimParse::parse(parts[2], error);
            if (error && !error->isEmpty()) {
                return false;
            }
        }
        if (parts.size() >= 4) {
            if (error) {
                *error = QStringLiteral("Unexpected extra Move field '%1'").arg(parts[3]);
            }
            return false;
        }
        return true;
    }

    bool anchorOk = false;
    const PageAnchor anchor = PageDimParse::parseAnchor(m0, &anchorOk);
    if (anchorOk && !looksNumeric(m0)) {
        out.moveMode = PageMoveMode::Direction;
        out.moveDirection = anchor;
        if (parts.size() >= 2 && !parts[1].isEmpty()) {
            if (!parseIntToken(parts[1], &out.moveAmount, error, "Move amount")) {
                return false;
            }
        }
        if (parts.size() >= 3) {
            if (error) {
                *error = QStringLiteral("Unexpected extra Move field '%1'").arg(parts[2]);
            }
            return false;
        }
        return true;
    }

    if (parts.size() >= 2) {
        out.moveMode = PageMoveMode::Absolute;
        out.moveX = PageDimParse::parse(parts[0], error);
        if (error && !error->isEmpty()) {
            return false;
        }
        out.moveY = PageDimParse::parse(parts[1], error);
        if (error && !error->isEmpty()) {
            return false;
        }
        if (parts.size() >= 3) {
            if (error) {
                *error = QStringLiteral("Unexpected extra Move field '%1'").arg(parts[2]);
            }
            return false;
        }
        return true;
    }

    if (error) {
        *error = QStringLiteral("Unknown Move value '%1'").arg(parts[0]);
    }
    return false;
}

bool parseMoveAndClickParts(const QStringList& parts, PageAction& out, QString* error)
{
    out.type = PageActionType::MoveAndClick;
    out.zoomMode = PageZoomMode::Off;
    if (!parts.isEmpty()) {
        out.button = parts[0];
    }
    if (parts.size() >= 2) {
        if (!parseZoomInt(parts[1], PageZoomMode::Off, out, error)) {
            return false;
        }
    }
    if (parts.size() >= 3) {
        if (error) {
            *error = QStringLiteral("Unexpected extra MoveAndClick field '%1'").arg(parts[2]);
        }
        return false;
    }
    return true;
}

bool parseNavParts(const ActionName& n, const QStringList& parts, PageAction& out, QString* error)
{
    applySpell(out, n);
    if (!parts.isEmpty()) {
        parseNavTarget(parts[0], out);
    }
    if (parts.size() >= 2 && !parts[1].isEmpty()) {
        bool flag = false;
        if (!PageDimParse::strictBool(parts[1], &flag)) {
            if (error) {
                *error = QStringLiteral("Expected true/false breadcrumb flag, got '%1'")
                             .arg(parts[1]);
            }
            return false;
        }
        out.breadcrumb = flag;
    }
    if (parts.size() >= 3) {
        if (error) {
            *error = QStringLiteral("Unexpected extra field '%1'").arg(parts[2]);
        }
        return false;
    }
    return true;
}

bool parseLegacyPageParts(const QStringList& parts, PageAction& out, QString* error)
{
    if (parts.size() < 2) {
        if (error) {
            *error = QStringLiteral("Page needs at least 2 value field(s)");
        }
        return false;
    }
    PageVerb v = PageVerb::Open;
    const QString verb = parts[0].toLower();
    if (verb == QLatin1String("open")) {
        v = PageVerb::Open;
    } else if (verb == QLatin1String("close")) {
        v = PageVerb::Close;
    } else if (verb == QLatin1String("toggle")) {
        v = PageVerb::Toggle;
    } else {
        if (error) {
            *error = QStringLiteral("Unknown Page verb '%1'").arg(parts[0]);
        }
        return false;
    }
    PageTargetKind k = PageTargetKind::Page;
    const QString kind = parts[1].toLower();
    if (kind == QLatin1String("page")) {
        k = PageTargetKind::Page;
    } else if (kind == QLatin1String("grid")) {
        k = PageTargetKind::Grid;
    } else if (kind == QLatin1String("zone")) {
        k = PageTargetKind::Zone;
    } else {
        if (error) {
            *error = QStringLiteral("Unknown Page target '%1'").arg(parts[1]);
        }
        return false;
    }
    out.type = PageActionType::Nav;
    out.verb = v;
    out.targetKind = k;
    if (parts.size() >= 3) {
        parseNavTarget(parts[2], out);
    }
    return true;
}

bool fillFromName(const ActionName& n, const QString& value, PageAction& out, QString* error)
{
    out.value = value;
    applySpell(out, n);
    const QStringList parts = splitCsv(value);
    switch (n.type) {
    case PageActionType::Send:
        return parseSendParts(parts, out, error);
    case PageActionType::Click:
        return parseClickParts(parts, out, error);
    case PageActionType::Move:
        return parseMoveParts(parts, out, error);
    case PageActionType::MoveAndClick:
        return parseMoveAndClickParts(parts, out, error);
    case PageActionType::Command:
        out.command = value.trimmed();
        return true;
    case PageActionType::Nav:
        return parseNavParts(n, parts, out, error);
    case PageActionType::GoBack:
        return true;
    case PageActionType::Speak:
        out.speakText = value;
        return true;
    case PageActionType::Ahk:
        return true;
    case PageActionType::Unknown:
        break;
    }
    if (error) {
        *error = QStringLiteral("Unknown action type");
    }
    return false;
}

QString actionValueFrom(const QXmlStreamAttributes& attrs, const QString& cdata)
{
    if (attrs.hasAttribute(QStringLiteral("value"))) {
        return attrs.value(QStringLiteral("value")).toString();
    }
    return cdata;
}

QVector<QPair<QString, QString>> collectActionAttrs(const QXmlStreamAttributes& attrs)
{
    QVector<QPair<QString, QString>> found;
    for (const ActionName& n : kNames) {
        const QString name = QString::fromLatin1(n.attr);
        if (attrs.hasAttribute(name)) {
            found.push_back({name, attrs.value(name).toString()});
        }
    }
    return found;
}

} // namespace

bool isPageActionElementName(QStringView name)
{
    const QString n = name.toString().trimmed();
    if (n.compare(QLatin1String("Action"), Qt::CaseInsensitive) == 0
        || n.compare(QLatin1String("AHK"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return findName(n) != nullptr;
}

QString pageActionAttributeName(const PageAction& a)
{
    if (const ActionName* n = findName(a)) {
        return QString::fromLatin1(n->attr);
    }
    return {};
}

QString pageActionElementName(const PageAction& a)
{
    if (a.type == PageActionType::Ahk) {
        return QStringLiteral("AHK");
    }
    if (const ActionName* n = findName(a)) {
        return QString::fromLatin1(n->element);
    }
    return QStringLiteral("Action");
}

QString pageActionSpell(const PageAction& a)
{
    if (a.type == PageActionType::Unknown) {
        return QStringLiteral("(none)");
    }
    if (a.type == PageActionType::Ahk) {
        return QStringLiteral("AHK");
    }
    if (const ActionName* n = findName(a)) {
        return QString::fromLatin1(n->element);
    }
    return QStringLiteral("(none)");
}

bool applyPageActionSpell(PageAction& a, const QString& spell)
{
    const QString s = spell.trimmed();
    if (s.isEmpty() || s == QLatin1String("(none)")) {
        a = PageAction{};
        return true;
    }
    if (s.compare(QLatin1String("AHK"), Qt::CaseInsensitive) == 0) {
        a = PageAction{};
        a.type = PageActionType::Ahk;
        return true;
    }
    const ActionName* n = findName(s);
    if (!n) {
        return false;
    }
    const PageActionType keep = n->type;
    a = PageAction{};
    applySpell(a, *n);
    if (keep == PageActionType::Move) {
        a.zoomMode = PageZoomMode::Settings;
    }
    return true;
}

QStringList pageActionSpells()
{
    QStringList out{QStringLiteral("(none)")};
    for (const ActionName& n : kNames) {
        out.push_back(QString::fromLatin1(n.element));
    }
    out.push_back(QStringLiteral("AHK"));
    return out;
}

QString pageActionValueText(const PageAction& a)
{
    switch (a.type) {
    case PageActionType::Send: {
        QStringList parts{a.sendKey};
        if (!a.sendEdge.isEmpty()) {
            parts.push_back(a.sendEdge);
        }
        if (a.sendDurationMs > 0) {
            parts.push_back(QString::number(a.sendDurationMs));
        }
        return csvJoin(parts);
    }
    case PageActionType::Click: {
        QStringList parts{a.button.isEmpty() ? QStringLiteral("left") : a.button};
        if (a.clickCount != 1 || a.speed != 0) {
            parts.push_back(QString::number(a.clickCount));
        }
        if (!a.clickEdge.isEmpty()) {
            parts.push_back(a.clickEdge);
        }
        if (a.speed != 0) {
            parts.push_back(QString::number(a.speed));
        }
        return csvJoin(parts);
    }
    case PageActionType::Move: {
        if (a.moveMode == PageMoveMode::Gaze) {
            if (a.zoomMode == PageZoomMode::Off) {
                return csvJoin({QStringLiteral("gaze"), QStringLiteral("0")});
            }
            if (a.zoomMode == PageZoomMode::Level) {
                return csvJoin({QStringLiteral("gaze"), QString::number(a.zoomLevel)});
            }
            return QStringLiteral("gaze");
        }
        if (a.moveMode == PageMoveMode::Direction) {
            QString dir;
            if (a.moveDirection == PageAnchor::Top) {
                dir = QStringLiteral("up");
            } else if (a.moveDirection == PageAnchor::Bottom) {
                dir = QStringLiteral("down");
            } else {
                dir = PageDimParse::anchorName(a.moveDirection);
            }
            if (a.moveAmount >= 0) {
                return csvJoin({dir, QString::number(a.moveAmount)});
            }
            return dir;
        }
        if (a.moveMode == PageMoveMode::Relative) {
            return csvJoin({QStringLiteral("relative"), PageDimParse::token(a.moveX),
                            PageDimParse::token(a.moveY)});
        }
        return csvJoin({PageDimParse::token(a.moveX), PageDimParse::token(a.moveY)});
    }
    case PageActionType::MoveAndClick: {
        const QString btn = a.button.isEmpty() ? QStringLiteral("left") : a.button;
        if (a.zoomMode == PageZoomMode::Level && a.zoomLevel > 0) {
            return csvJoin({btn, QString::number(a.zoomLevel)});
        }
        if (a.zoomMode == PageZoomMode::Off) {
            return btn;
        }
        return csvJoin({btn, QString::number(qMax(1, a.zoomLevel))});
    }
    case PageActionType::Command:
        return a.command;
    case PageActionType::Nav: {
        const QString id = navTargetText(a);
        if (a.breadcrumb) {
            return csvJoin({id, QStringLiteral("true")});
        }
        return id;
    }
    case PageActionType::GoBack:
        return {};
    case PageActionType::Speak:
        return a.speakText;
    case PageActionType::Ahk:
    case PageActionType::Unknown:
        return a.value;
    }
    return {};
}

bool pageActionCanInline(const PageAction& a)
{
    if (a.type == PageActionType::Ahk || a.type == PageActionType::Unknown) {
        return false;
    }
    if (a.type == PageActionType::Command && !a.args.isEmpty()) {
        return false;
    }
    return !pageActionAttributeName(a).isEmpty();
}

bool parsePageActionAttribute(QStringView name, QStringView value, PageAction& out, QString* error)
{
    const ActionName* n = findName(name);
    if (!n) {
        if (error) {
            *error = QStringLiteral("Unknown action attribute '%1'").arg(name);
        }
        return false;
    }
    out = PageAction{};
    return fillFromName(*n, value.toString(), out, error);
}

bool takePageActionAttributes(const QXmlStreamAttributes& attrs, QVector<PageAction>& actions,
                              QString* error)
{
    const QVector<QPair<QString, QString>> found = collectActionAttrs(attrs);
    if (found.size() > 1) {
        if (error) {
            *error = QStringLiteral("Only 1 action attribute is allowed on a Cell or Zone");
        }
        return false;
    }
    if (found.isEmpty()) {
        return true;
    }
    PageAction act;
    if (!parsePageActionAttribute(found[0].first, found[0].second, act, error)) {
        return false;
    }
    if (act.type == PageActionType::Command && attrs.hasAttribute(QStringLiteral("args"))) {
        act.args = attrs.value(QStringLiteral("args")).toString();
    }
    actions.push_back(act);
    return true;
}

bool parsePageAction(const QXmlStreamAttributes& a, const QString& cdata, PageAction& out,
                     QString* error)
{
    out = PageAction{};
    const QVector<QPair<QString, QString>> found = collectActionAttrs(a);
    if (found.size() > 1) {
        if (error) {
            *error = QStringLiteral("Only 1 action attribute is allowed on Action");
        }
        return false;
    }
    if (found.size() == 1) {
        if (!parsePageActionAttribute(found[0].first, found[0].second, out, error)) {
            return false;
        }
        if (a.hasAttribute(QStringLiteral("args"))) {
            out.args = a.value(QStringLiteral("args")).toString();
        }
        return true;
    }

    const QString id = a.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Action needs an action attribute or id");
        }
        return false;
    }
    const QString value = actionValueFrom(a, cdata);
    out.args = a.value(QStringLiteral("args")).toString();
    out.ahkSource = cdata;
    out.value = value;

    if (id.toLower() == QLatin1String("page")) {
        return parseLegacyPageParts(splitCsv(value), out, error);
    }
    if (id.toLower() == QLatin1String("ahk")) {
        out.type = PageActionType::Ahk;
        return true;
    }
    const ActionName* n = findName(id);
    if (!n) {
        if (error) {
            *error = QStringLiteral("Unknown Action id '%1'").arg(id);
        }
        return false;
    }
    if (!fillFromName(*n, value, out, error)) {
        return false;
    }
    if (n->type == PageActionType::Command && out.command.isEmpty()) {
        out.command = out.args.trimmed();
    }
    return true;
}

bool parsePageActionElement(QStringView elementName, const QXmlStreamAttributes& attrs,
                            const QString& cdata, PageAction& out, QString* error)
{
    const QString name = elementName.toString();
    if (name.compare(QLatin1String("Action"), Qt::CaseInsensitive) == 0) {
        return parsePageAction(attrs, cdata, out, error);
    }
    if (name.compare(QLatin1String("AHK"), Qt::CaseInsensitive) == 0) {
        out = PageAction{};
        out.type = PageActionType::Ahk;
        out.ahkSource = cdata;
        return true;
    }
    const ActionName* n = findName(name);
    if (!n) {
        if (error) {
            *error = QStringLiteral("Unknown action '%1'").arg(name);
        }
        return false;
    }
    out = PageAction{};
    if (!fillFromName(*n, actionValueFrom(attrs, cdata), out, error)) {
        return false;
    }
    if (attrs.hasAttribute(QStringLiteral("args"))) {
        out.args = attrs.value(QStringLiteral("args")).toString();
        if (n->type == PageActionType::Command && out.command.isEmpty()) {
            out.command = out.args.trimmed();
        }
    }
    return true;
}

} // namespace gazer

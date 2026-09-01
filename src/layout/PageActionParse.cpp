#include "layout/PageActionParse.h"

#include "layout/PageActionParseFields.h"
#include "layout/PageDim.h"

#include <QPair>
#include <QStringList>
#include <QVector>
#include <QXmlStreamAttribute>

namespace gazer {

namespace {

using pageaction::buttonKey;
using pageaction::compassToken;
using pageaction::csvJoin;
using pageaction::clickKindText;
using pageaction::navTargetText;
using pageaction::parseClickKindToken;
using pageaction::parseClickKindValue;
using pageaction::parseCommandValue;
using pageaction::parseGazeClick;
using pageaction::parseGazeMove;
using pageaction::parseLegacyClick;
using pageaction::parseLegacyMove;
using pageaction::parseLegacyMoveAndClick;
using pageaction::parseLegacyPageParts;
using pageaction::parseMoveDir;
using pageaction::parseMovePoint;
using pageaction::parseNavValue;
using pageaction::parseLayersValue;
using pageaction::parseSendValue;
using pageaction::parseSpeakValue;
using pageaction::parseZoomSpec;
using pageaction::splitCsv;
using pageaction::zoomSpecText;


struct ActionName;

using ParseFn = bool (*)(const QString& value, PageAction& out, QString* error);
using MatchFn = bool (*)(const PageAction& a, const ActionName& n);

struct ActionName {
    const char* attr;
    const char* element;
    PageActionType type;
    PageVerb verb = PageVerb::Open;
    const char* button = nullptr;
    bool listed = true;
    ParseFn parse = nullptr;
    MatchFn match = nullptr;
    PageNavScope scope = PageNavScope::Id;
};



bool matchButton(const PageAction& a, const ActionName& n)
{
    const QString want = n.button ? QString::fromLatin1(n.button) : QStringLiteral("left");
    return buttonKey(a.button) == want;
}

bool matchGaze(const PageAction& a, const ActionName&)
{
    return a.moveMode == PageMoveMode::Gaze;
}

bool matchDir(const PageAction& a, const ActionName&)
{
    return a.moveMode == PageMoveMode::Direction;
}

bool matchPoint(const PageAction& a, const ActionName&)
{
    return a.moveMode == PageMoveMode::Absolute || a.moveMode == PageMoveMode::Relative;
}

bool matchNavScope(const PageAction& a, const ActionName& n)
{
    return a.targetScope == n.scope;
}

void applyBasics(PageAction& out, const ActionName& n)
{
    out.type = n.type;
    if (n.button) {
        out.button = QString::fromLatin1(n.button);
    }
    if (n.type == PageActionType::Nav) {
        out.verb = n.verb;
        out.targetScope = n.scope;
    }
}

const ActionName kNames[] = {
    {"send", "Send", PageActionType::Send, PageVerb::Open, nullptr, true, parseSendValue},
    {"leftClick", "LeftClick", PageActionType::Click, PageVerb::Open, "left", true,
     parseClickKindValue, matchButton},
    {"middleClick", "MiddleClick", PageActionType::Click, PageVerb::Open, "middle", true,
     parseClickKindValue, matchButton},
    {"rightClick", "RightClick", PageActionType::Click, PageVerb::Open, "right", true,
     parseClickKindValue, matchButton},
    {"leftClickAtGaze", "LeftClickAtGaze", PageActionType::MoveAndClick, PageVerb::Open, "left",
     true, parseGazeClick, matchButton},
    {"middleClickAtGaze", "MiddleClickAtGaze", PageActionType::MoveAndClick, PageVerb::Open,
     "middle", true, parseGazeClick, matchButton},
    {"rightClickAtGaze", "RightClickAtGaze", PageActionType::MoveAndClick, PageVerb::Open, "right",
     true, parseGazeClick, matchButton},
    {"mouseMoveByDirection", "MouseMoveByDirection", PageActionType::Move, PageVerb::Open, nullptr,
     true, parseMoveDir, matchDir},
    {"mouseMoveToGaze", "MouseMoveToGaze", PageActionType::Move, PageVerb::Open, nullptr, true,
     parseGazeMove, matchGaze},
    {"mouseMoveToPoint", "MouseMoveToPoint", PageActionType::Move, PageVerb::Open, nullptr, true,
     parseMovePoint, matchPoint},
    {"click", "Click", PageActionType::Click, PageVerb::Open, nullptr, false, parseLegacyClick},
    {"move", "Move", PageActionType::Move, PageVerb::Open, nullptr, false, parseLegacyMove},
    {"moveAndClick", "MoveAndClick", PageActionType::MoveAndClick, PageVerb::Open, nullptr, false,
     parseLegacyMoveAndClick},
    {"command", "Command", PageActionType::Command, PageVerb::Open, nullptr, true,
     parseCommandValue},
    {"openPage", "OpenPage", PageActionType::Nav, PageVerb::Open, nullptr, true, parseNavValue},
    {"closePage", "ClosePage", PageActionType::Nav, PageVerb::Close, nullptr, true, nullptr,
     matchNavScope, PageNavScope::Self},
    {"closeAllPages", "CloseAllPages", PageActionType::Nav, PageVerb::Close, nullptr, true, nullptr,
     matchNavScope, PageNavScope::All},
    {"closeOtherPages", "CloseOtherPages", PageActionType::Nav, PageVerb::Close, nullptr, true,
     nullptr, matchNavScope, PageNavScope::Others},
    {"togglePage", "TogglePage", PageActionType::Nav, PageVerb::Toggle, nullptr, true,
     parseNavValue},
    {"showLayers", "ShowLayers", PageActionType::ShowLayers, PageVerb::Open, nullptr, true,
     parseLayersValue},
    {"goBack", "GoBack", PageActionType::GoBack},
    {"speak", "Speak", PageActionType::Speak, PageVerb::Open, nullptr, true, parseSpeakValue},
};

const ActionName kAliases[] = {
    {"mouseClickLeft", "MouseClickLeft", PageActionType::Click, PageVerb::Open, "left", false,
     parseClickKindValue, matchButton},
    {"mouseLeftClick", "MouseLeftClick", PageActionType::Click, PageVerb::Open, "left", false,
     parseClickKindValue, matchButton},
    {"mouseClickMiddle", "MouseClickMiddle", PageActionType::Click, PageVerb::Open, "middle", false,
     parseClickKindValue, matchButton},
    {"mouseMiddleClick", "MouseMiddleClick", PageActionType::Click, PageVerb::Open, "middle", false,
     parseClickKindValue, matchButton},
    {"mouseClickRight", "MouseClickRight", PageActionType::Click, PageVerb::Open, "right", false,
     parseClickKindValue, matchButton},
    {"mouseRightClick", "MouseRightClick", PageActionType::Click, PageVerb::Open, "right", false,
     parseClickKindValue, matchButton},
    {"mouseClickAtGazeLeft", "MouseClickAtGazeLeft", PageActionType::MoveAndClick, PageVerb::Open,
     "left", false, parseGazeClick, matchButton},
    {"mouseMoveAndLeftClick", "MouseMoveAndLeftClick", PageActionType::MoveAndClick, PageVerb::Open,
     "left", false, parseGazeClick, matchButton},
    {"mouseClickAtGazeMiddle", "MouseClickAtGazeMiddle", PageActionType::MoveAndClick,
     PageVerb::Open, "middle", false, parseGazeClick, matchButton},
    {"mouseMoveAndMiddleClick", "MouseMoveAndMiddleClick", PageActionType::MoveAndClick,
     PageVerb::Open, "middle", false, parseGazeClick, matchButton},
    {"mouseClickAtGazeRight", "MouseClickAtGazeRight", PageActionType::MoveAndClick, PageVerb::Open,
     "right", false, parseGazeClick, matchButton},
    {"mouseMoveAndRightClick", "MouseMoveAndRightClick", PageActionType::MoveAndClick,
     PageVerb::Open, "right", false, parseGazeClick, matchButton},
};

const ActionName* findName(QStringView raw)
{
    const QString n = raw.toString().trimmed().toLower();
    if (n.isEmpty() || n == QLatin1String("action") || n == QLatin1String("page")) {
        return nullptr;
    }
    auto match = [&](const ActionName& a) {
        return n == QLatin1String(a.attr) || n == QString::fromLatin1(a.element).toLower();
    };
    for (const ActionName& a : kNames) {
        if (match(a)) {
            return &a;
        }
    }
    for (const ActionName& a : kAliases) {
        if (match(a)) {
            return &a;
        }
    }
    return nullptr;
}

const ActionName* findName(const PageAction& a)
{
    const ActionName* legacy = nullptr;
    for (const ActionName& n : kNames) {
        if (n.type != a.type) {
            continue;
        }
        if (a.type == PageActionType::Nav && n.verb != a.verb) {
            continue;
        }
        if (!n.listed) {
            if (!legacy) {
                legacy = &n;
            }
            continue;
        }
        if (n.match && !n.match(a, n)) {
            continue;
        }
        return &n;
    }
    return legacy;
}

bool fillFromName(const ActionName& n, const QString& value, PageAction& out, QString* error)
{
    out.value = value;
    applyBasics(out, n);
    if (!n.parse) {
        return true;
    }
    return n.parse(value, out, error);
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
    for (const QXmlStreamAttribute& a : attrs) {
        if (const ActionName* n = findName(a.name())) {
            found.push_back({QString::fromLatin1(n->attr), a.value().toString()});
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
    a = PageAction{};
    applyBasics(a, *n);
    if (n->parse) {
        QString ignored;
        (void)n->parse(QString(), a, &ignored);
    }
    return true;
}

QStringList pageActionSpells()
{
    QStringList out{QStringLiteral("(none)")};
    for (const ActionName& n : kNames) {
        if (!n.listed) {
            continue;
        }
        out.push_back(QString::fromLatin1(n.element));
    }
    out.push_back(QStringLiteral("AHK"));
    return out;
}

QString pageActionClickKindText(PageClickKind k)
{
    const QString t = clickKindText(k);
    return t.isEmpty() ? QStringLiteral("default") : t;
}

QStringList pageActionClickKindChoices()
{
    return {QStringLiteral("default"), QStringLiteral("double"), QStringLiteral("down"),
            QStringLiteral("up"), QStringLiteral("toggle")};
}

bool applyPageActionClickKind(PageAction& a, const QString& token, QString* error)
{
    return parseClickKindToken(token, a, error);
}

QString pageActionZoomText(const PageAction& a)
{
    const QString t = zoomSpecText(a);
    return t.isEmpty() ? QStringLiteral("default") : t;
}

QStringList pageActionZoomChoices()
{
    return {QStringLiteral("default"), QStringLiteral("0"), QStringLiteral("-1"),
            QStringLiteral("-2"),      QStringLiteral("2"), QStringLiteral("3"),
            QStringLiteral("4"),       QStringLiteral("5"), QStringLiteral("6")};
}

bool applyPageActionZoom(PageAction& a, const QString& token, QString* error)
{
    return parseZoomSpec(token, a, error);
}

QString pageActionCompassToken(PageAnchor a)
{
    return compassToken(a);
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
    case PageActionType::Click:
        return clickKindText(a.clickKind);
    case PageActionType::Move: {
        if (a.moveMode == PageMoveMode::Gaze) {
            return zoomSpecText(a);
        }
        if (a.moveMode == PageMoveMode::Direction) {
            const QString dir = compassToken(a.moveDirection);
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
    case PageActionType::MoveAndClick:
        return zoomSpecText(a);
    case PageActionType::Command:
        return a.command;
    case PageActionType::Nav: {
        if (a.verb == PageVerb::Close && a.targetScope != PageNavScope::Id) {
            return {};
        }
        const QString id = navTargetText(a);
        if (a.breadcrumb) {
            return csvJoin({id, QStringLiteral("true")});
        }
        return id;
    }
    case PageActionType::ShowLayers:
        return layerListCsv(normalizedLayers(a.layers));
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

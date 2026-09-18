#include "layout/PageActionParseFields.h"

#include "layout/PageDim.h"

#include <QtGlobal>

namespace gazer {
namespace pageaction {

namespace {

bool noExtra(const QStringList& p, int n, QString* error)
{
    if (p.size() <= n) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("Unexpected extra field '%1'").arg(p[n]);
    }
    return false;
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

} // namespace

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
    return QStringList(parts.mid(0, last)).join(QStringLiteral(", "));
}

QString buttonKey(const QString& button)
{
    const QString b = button.trimmed().toLower();
    if (b == QLatin1String("right") || b == QLatin1String("middle")) {
        return b;
    }
    return QStringLiteral("left");
}

bool parseClickKindToken(const QString& tok, PageAction& out, QString* error)
{
    const QString t = tok.trimmed().toLower();
    if (t.isEmpty() || t == QLatin1String("default") || t == QLatin1String("single")) {
        out.clickKind = PageClickKind::Default;
        return true;
    }
    if (t == QLatin1String("double")) {
        out.clickKind = PageClickKind::Double;
        return true;
    }
    if (t == QLatin1String("down")) {
        out.clickKind = PageClickKind::Down;
        return true;
    }
    if (t == QLatin1String("up")) {
        out.clickKind = PageClickKind::Up;
        return true;
    }
    if (t == QLatin1String("toggle")) {
        out.clickKind = PageClickKind::Toggle;
        return true;
    }
    if (error) {
        *error = QStringLiteral("Unknown click type '%1' (default/double/down/up/toggle)")
                     .arg(tok);
    }
    return false;
}

bool parseZoomSpec(const QString& tok, PageAction& out, QString* error)
{
    const QString t = tok.trimmed().toLower();
    if (t.isEmpty() || t == QLatin1String("default")) {
        out.zoomMode = PageZoomMode::Settings;
        out.zoomLevel = 0;
        return true;
    }
    int z = 0;
    if (!parseIntToken(tok, &z, error, "Zoom")) {
        return false;
    }
    if (z == 0) {
        out.zoomMode = PageZoomMode::Off;
        out.zoomLevel = 0;
    } else if (z == -1) {
        out.zoomMode = PageZoomMode::Foresight;
        out.zoomLevel = 0;
    } else if (z == -2) {
        out.zoomMode = PageZoomMode::ForesightBonus;
        out.zoomLevel = 0;
    } else if (z > 0) {
        out.zoomMode = PageZoomMode::Level;
        out.zoomLevel = z;
    } else {
        if (error) {
            *error = QStringLiteral("Zoom must be default, -2, -1, 0, or a positive zoom");
        }
        return false;
    }
    return true;
}

bool parseZoomValue(const QString& value, PageAction& out, QString* error)
{
    const QStringList parts = splitCsv(value);
    if (!noExtra(parts, 1, error)) {
        return false;
    }
    return parseZoomSpec(parts.isEmpty() ? QString() : parts[0], out, error);
}

bool parseClickKindValue(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::Click;
    const QStringList parts = splitCsv(value);
    if (!noExtra(parts, 1, error)) {
        return false;
    }
    return parseClickKindToken(parts.isEmpty() ? QString() : parts[0], out, error);
}

bool parseGazeMove(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::Move;
    out.moveMode = PageMoveMode::Gaze;
    return parseZoomValue(value, out, error);
}

bool parseGazeClick(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::MoveAndClick;
    return parseZoomValue(value, out, error);
}

bool parseMoveDir(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::Move;
    out.moveMode = PageMoveMode::Direction;
    const QStringList parts = splitCsv(value);
    if (parts.isEmpty()) {
        return true;
    }
    bool ok = false;
    out.moveDirection = PageDimParse::parseAnchor(parts[0], &ok);
    if (!ok || looksNumeric(parts[0])) {
        if (error) {
            *error = QStringLiteral("Unknown direction '%1'").arg(parts[0]);
        }
        return false;
    }
    if (parts.size() >= 2 && !parts[1].isEmpty()
        && !parseIntToken(parts[1], &out.moveAmount, error, "Move amount")) {
        return false;
    }
    return noExtra(parts, 2, error);
}

bool parseMovePoint(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::Move;
    const QStringList parts = splitCsv(value);
    int i = 0;
    if (!parts.isEmpty() && parts[0].trimmed().toLower() == QLatin1String("relative")) {
        out.moveMode = PageMoveMode::Relative;
        i = 1;
    } else {
        out.moveMode = PageMoveMode::Absolute;
    }
    if (parts.size() - i == 0) {
        return true;
    }
    if (parts.size() - i < 2) {
        if (error) {
            *error = QStringLiteral("MouseMoveToPoint needs x,y");
        }
        return false;
    }
    out.moveX = PageDimParse::parse(parts[i], error);
    if (error && !error->isEmpty()) {
        return false;
    }
    out.moveY = PageDimParse::parse(parts[i + 1], error);
    if (error && !error->isEmpty()) {
        return false;
    }
    return noExtra(parts, i + 2, error);
}

bool parseSendValue(const QString& raw, PageAction& out, QString* error)
{
    out.type = PageActionType::Send;
    const QString s = raw.trimmed();
    QString rest;
    if (s.startsWith(QLatin1Char(','))) {
        out.sendKey = QStringLiteral(",");
        rest = s.mid(1);
        if (rest.startsWith(QLatin1Char(','))) {
            rest = rest.mid(1);
        }
        rest = rest.trimmed();
    } else {
        const int cut = s.indexOf(QLatin1Char(','));
        if (cut < 0) {
            out.sendKey = s;
            return true;
        }
        out.sendKey = s.left(cut).trimmed();
        rest = s.mid(cut + 1).trimmed();
    }
    if (rest.isEmpty()) {
        return true;
    }
    const QStringList tail = splitCsv(rest);
    int i = 0;
    if (i < tail.size() && isEdgeToken(tail[i])) {
        out.sendEdge = tail[i++];
    }
    if (i < tail.size()) {
        if (!parseIntToken(tail[i], &out.sendDurationMs, error, "Send duration")) {
            return false;
        }
        ++i;
    }
    return noExtra(tail, i, error);
}

bool parseCommandValue(const QString& value, PageAction& out, QString* error)
{
    Q_UNUSED(error);
    out.command = value.trimmed();
    return true;
}

bool parseSpeakValue(const QString& value, PageAction& out, QString* error)
{
    Q_UNUSED(error);
    out.speakText = value;
    return true;
}

bool parseNavValue(const QString& value, PageAction& out, QString* error)
{
    const QStringList parts = splitCsv(value);
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
    return noExtra(parts, 2, error);
}

bool parseHostPageValue(const QString& value, PageAction& out, QString* error)
{
    const QStringList parts = splitCsv(value);
    if (parts.size() >= 2) {
        out.hostId = parts[0];
        out.targetId = parts[1];
        return noExtra(parts, 2, error);
    }
    out.hostId.clear();
    out.targetId = parts.isEmpty() ? QString() : parts[0];
    return noExtra(parts, 1, error);
}

bool parseLayersValue(const QString& value, PageAction& out, QString* error)
{
    const QString t = value.trimmed();
    if (t.isEmpty()) {
        out.layers = defaultLayers();
        return true;
    }
    if (!parseLayerListStrict(t, out.layers) || out.layers.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Invalid ShowLayers value '%1'").arg(value);
        }
        return false;
    }
    return true;
}

bool parseLegacyClick(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::Click;
    const QStringList parts = splitCsv(value);
    if (!parts.isEmpty()) {
        out.button = parts[0];
    }
    int i = 1;
    PageClickKind kind = PageClickKind::Default;
    if (i < parts.size() && !isEdgeToken(parts[i]) && !parts[i].isEmpty()) {
        int count = 1;
        if (!parseIntToken(parts[i], &count, error, "Click count")) {
            return false;
        }
        if (count >= 2) {
            kind = PageClickKind::Double;
        }
        ++i;
    }
    if (i < parts.size() && isEdgeToken(parts[i])) {
        kind = parts[i].trimmed().toLower() == QLatin1String("down") ? PageClickKind::Down
                                                                     : PageClickKind::Up;
        ++i;
    } else if (i < parts.size() && parts[i].isEmpty()) {
        ++i;
    }
    if (i < parts.size() && !parts[i].isEmpty()) {
        int ignoredSpeed = 0;
        if (!parseIntToken(parts[i], &ignoredSpeed, error, "Click speed")) {
            return false;
        }
        ++i;
    }
    if (!noExtra(parts, i, error)) {
        return false;
    }
    out.clickKind = kind;
    return true;
}

bool parseLegacyMove(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::Move;
    const QStringList parts = splitCsv(value);
    if (parts.isEmpty()) {
        out.moveMode = PageMoveMode::Gaze;
        out.zoomMode = PageZoomMode::Settings;
        return true;
    }
    const QString l0 = parts[0].trimmed().toLower();
    if (l0 == QLatin1String("gaze")) {
        out.moveMode = PageMoveMode::Gaze;
        return parseZoomSpec(parts.size() >= 2 ? parts[1] : QString(), out, error)
               && noExtra(parts, 2, error);
    }
    if (l0 == QLatin1String("relative") || l0 == QLatin1String("absolute")) {
        return parseMovePoint(value, out, error);
    }
    bool anchorOk = false;
    (void)PageDimParse::parseAnchor(parts[0], &anchorOk);
    if (anchorOk && !looksNumeric(parts[0])) {
        return parseMoveDir(value, out, error);
    }
    if (parts.size() >= 2) {
        return parseMovePoint(value, out, error);
    }
    if (error) {
        *error = QStringLiteral("Unknown Move value '%1'").arg(parts[0]);
    }
    return false;
}

bool parseLegacyMoveAndClick(const QString& value, PageAction& out, QString* error)
{
    out.type = PageActionType::MoveAndClick;
    out.zoomMode = PageZoomMode::Settings;
    const QStringList parts = splitCsv(value);
    if (!parts.isEmpty()) {
        out.button = parts[0];
    }
    if (parts.size() >= 2 && !parseZoomSpec(parts[1], out, error)) {
        return false;
    }
    return noExtra(parts, 2, error);
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
    if (verb == QLatin1String("open") || verb == QLatin1String("show")) {
        v = PageVerb::Open;
    } else if (verb == QLatin1String("close") || verb == QLatin1String("hide")) {
        v = PageVerb::Close;
    } else if (verb == QLatin1String("toggle")) {
        v = PageVerb::Toggle;
    } else {
        if (error) {
            *error = QStringLiteral("Unknown Page verb '%1'").arg(parts[0]);
        }
        return false;
    }
    const QString kind = parts[1].toLower();
    if (kind == QLatin1String("layers") || kind == QLatin1String("layer")) {
        QStringList rest;
        for (int i = 2; i < parts.size(); ++i) {
            rest.push_back(parts[i]);
        }
        out.type = PageActionType::ShowLayers;
        const QString csv = rest.join(QLatin1Char(','));
        if (csv.trimmed().isEmpty()) {
            out.layers = defaultLayers();
            return true;
        }
        if (!parseLayerListStrict(csv, out.layers) || out.layers.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Invalid ShowLayers value '%1'").arg(csv);
            }
            return false;
        }
        return true;
    }
    if (kind != QLatin1String("page")) {
        if (error) {
            *error = QStringLiteral("Unknown Page target '%1' (page/layers)").arg(parts[1]);
        }
        return false;
    }
    out.type = PageActionType::Nav;
    out.verb = v;
    if (parts.size() >= 3) {
        parseNavTarget(parts[2], out);
    }
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

QString clickKindText(PageClickKind k)
{
    switch (k) {
    case PageClickKind::Double:
        return QStringLiteral("double");
    case PageClickKind::Down:
        return QStringLiteral("down");
    case PageClickKind::Up:
        return QStringLiteral("up");
    case PageClickKind::Toggle:
        return QStringLiteral("toggle");
    case PageClickKind::Default:
        break;
    }
    return {};
}

QString zoomSpecText(const PageAction& a)
{
    switch (a.zoomMode) {
    case PageZoomMode::Off:
        return QStringLiteral("0");
    case PageZoomMode::Foresight:
        return QStringLiteral("-1");
    case PageZoomMode::ForesightBonus:
        return QStringLiteral("-2");
    case PageZoomMode::Level:
        return QString::number(a.zoomLevel);
    case PageZoomMode::Settings:
        break;
    }
    return {};
}

QString compassToken(PageAnchor a)
{
    switch (a) {
    case PageAnchor::Top:
        return QStringLiteral("n");
    case PageAnchor::Bottom:
        return QStringLiteral("s");
    case PageAnchor::Right:
        return QStringLiteral("e");
    case PageAnchor::Left:
        return QStringLiteral("w");
    case PageAnchor::TopRight:
        return QStringLiteral("ne");
    case PageAnchor::TopLeft:
        return QStringLiteral("nw");
    case PageAnchor::BottomRight:
        return QStringLiteral("se");
    case PageAnchor::BottomLeft:
        return QStringLiteral("sw");
    case PageAnchor::Center:
        break;
    }
    return PageDimParse::anchorName(a);
}

} // namespace pageaction
} // namespace gazer

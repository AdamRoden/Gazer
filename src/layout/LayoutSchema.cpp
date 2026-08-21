#include "layout/LayoutSchema.h"

#include "utils/Log.h"

#include <QHash>

namespace gazer {
namespace LayoutSchema {

namespace {

QString norm(const QString& s)
{
    return s.trimmed().toLower();
}

} // namespace

QString actionTypeName(LayoutAction::Type t)
{
    switch (t) {
    case LayoutAction::Type::Speak:
        return QStringLiteral("speak");
    case LayoutAction::Type::TypeText:
        return QStringLiteral("typeText");
    case LayoutAction::Type::LoadLayout:
        return QStringLiteral("loadLayout");
    case LayoutAction::Type::OpenLayout:
        return QStringLiteral("openLayout");
    case LayoutAction::Type::CloseLayout:
        return QStringLiteral("closeLayout");
    case LayoutAction::Type::Command:
        return QStringLiteral("command");
    case LayoutAction::Type::Script:
        return QStringLiteral("script");
    case LayoutAction::Type::Unknown:
        break;
    }
    return {};
}

LayoutAction::Type actionTypeFromName(const QString& s)
{
    static const QHash<QString, LayoutAction::Type> kTypes = {
        {QStringLiteral("speak"), LayoutAction::Type::Speak},
        {QStringLiteral("typetext"), LayoutAction::Type::TypeText},
        {QStringLiteral("loadlayout"), LayoutAction::Type::LoadLayout},
        {QStringLiteral("openlayout"), LayoutAction::Type::OpenLayout},
        {QStringLiteral("closelayout"), LayoutAction::Type::CloseLayout},
        {QStringLiteral("command"), LayoutAction::Type::Command},
        {QStringLiteral("script"), LayoutAction::Type::Script},
    };
    return kTypes.value(norm(s), LayoutAction::Type::Unknown);
}

QStringList actionTypeNames()
{
    return {QStringLiteral("speak"),      QStringLiteral("typeText"),
            QStringLiteral("loadLayout"), QStringLiteral("openLayout"),
            QStringLiteral("closeLayout"), QStringLiteral("command"),
            QStringLiteral("script")};
}

QString windowAnchorName(LayoutWindowPlacement::Anchor a)
{
    using A = LayoutWindowPlacement::Anchor;
    switch (a) {
    case A::TopLeft:
        return QStringLiteral("topLeft");
    case A::TopCenter:
        return QStringLiteral("topCenter");
    case A::TopRight:
        return QStringLiteral("topRight");
    case A::Center:
        return QStringLiteral("center");
    case A::LeftCenter:
        return QStringLiteral("leftCenter");
    case A::RightCenter:
        return QStringLiteral("rightCenter");
    case A::BottomLeft:
        return QStringLiteral("bottomLeft");
    case A::BottomCenter:
        return QStringLiteral("bottomCenter");
    case A::BottomRight:
        return QStringLiteral("bottomRight");
    case A::Default:
        break;
    }
    return QStringLiteral("default");
}

LayoutWindowPlacement::Anchor windowAnchorFromName(const QString& s)
{
    using A = LayoutWindowPlacement::Anchor;
    static const QHash<QString, A> kAnchors = {
        {QStringLiteral("topleft"), A::TopLeft},
        {QStringLiteral("topcenter"), A::TopCenter},
        {QStringLiteral("topright"), A::TopRight},
        {QStringLiteral("center"), A::Center},
        {QStringLiteral("leftcenter"), A::LeftCenter},
        {QStringLiteral("centerleft"), A::LeftCenter},
        {QStringLiteral("rightcenter"), A::RightCenter},
        {QStringLiteral("centerright"), A::RightCenter},
        {QStringLiteral("bottomleft"), A::BottomLeft},
        {QStringLiteral("bottomcenter"), A::BottomCenter},
        {QStringLiteral("bottomright"), A::BottomRight},
        {QStringLiteral("default"), A::Default},
        {QString(), A::Default},
    };
    const QString n = norm(s);
    if (!n.isEmpty() && !kAnchors.contains(n)) {
        GAZER_WARN << "Unknown window.anchor" << s << "— using default";
    }
    return kAnchors.value(n, A::Default);
}

QStringList windowAnchorNames()
{
    return {QStringLiteral("default"),     QStringLiteral("topLeft"),
            QStringLiteral("topCenter"),   QStringLiteral("topRight"),
            QStringLiteral("center"),      QStringLiteral("leftCenter"),
            QStringLiteral("rightCenter"), QStringLiteral("bottomLeft"),
            QStringLiteral("bottomCenter"), QStringLiteral("bottomRight")};
}

QString screenAnchorName(LayoutDwellRegion::ScreenAnchor a)
{
    using SA = LayoutDwellRegion::ScreenAnchor;
    switch (a) {
    case SA::Top:
        return QStringLiteral("top");
    case SA::Bottom:
        return QStringLiteral("bottom");
    case SA::Left:
        return QStringLiteral("left");
    case SA::Right:
        return QStringLiteral("right");
    case SA::TopLeft:
        return QStringLiteral("topLeft");
    case SA::TopRight:
        return QStringLiteral("topRight");
    case SA::BottomLeft:
        return QStringLiteral("bottomLeft");
    case SA::BottomRight:
        return QStringLiteral("bottomRight");
    case SA::TopCenter:
        return QStringLiteral("topCenter");
    case SA::BottomCenter:
        return QStringLiteral("bottomCenter");
    case SA::LeftCenter:
        return QStringLiteral("leftCenter");
    case SA::RightCenter:
        return QStringLiteral("rightCenter");
    case SA::None:
        break;
    }
    return {};
}

LayoutDwellRegion::ScreenAnchor screenAnchorFromName(const QString& s)
{
    using SA = LayoutDwellRegion::ScreenAnchor;
    static const QHash<QString, SA> kAnchors = {
        {QStringLiteral("top"), SA::Top},
        {QStringLiteral("bottom"), SA::Bottom},
        {QStringLiteral("left"), SA::Left},
        {QStringLiteral("right"), SA::Right},
        {QStringLiteral("topleft"), SA::TopLeft},
        {QStringLiteral("topright"), SA::TopRight},
        {QStringLiteral("bottomleft"), SA::BottomLeft},
        {QStringLiteral("bottomright"), SA::BottomRight},
        {QStringLiteral("topcenter"), SA::TopCenter},
        {QStringLiteral("bottomcenter"), SA::BottomCenter},
        {QStringLiteral("leftcenter"), SA::LeftCenter},
        {QStringLiteral("rightcenter"), SA::RightCenter},
    };
    return kAnchors.value(norm(s), SA::None);
}

QStringList screenAnchorNames()
{
    return {QStringLiteral("top"),          QStringLiteral("bottom"),
            QStringLiteral("left"),         QStringLiteral("right"),
            QStringLiteral("topLeft"),      QStringLiteral("topRight"),
            QStringLiteral("bottomLeft"),   QStringLiteral("bottomRight"),
            QStringLiteral("topCenter"),    QStringLiteral("bottomCenter"),
            QStringLiteral("leftCenter"),   QStringLiteral("rightCenter")};
}

QStringList itemAnchorNames()
{
    QStringList names;
    names.push_back(QStringLiteral("cell"));
    names += screenAnchorNames();
    return names;
}

QString itemAnchorName(const LayoutItem& item)
{
    if (!item.isUnbounded()) {
        return QStringLiteral("cell");
    }
    return screenAnchorName(item.dwellRegion.screenAnchor);
}

LayoutDwellRegion::ScreenAnchor itemAnchorFromName(const QString& s)
{
    const QString n = norm(s);
    if (n == QLatin1String("cell") || n == QLatin1String("grid") || s.trimmed().isEmpty()) {
        return LayoutDwellRegion::ScreenAnchor::None;
    }
    return screenAnchorFromName(s);
}

QString boundsModeName(BoundsMode m)
{
    return m == BoundsMode::Screen ? QStringLiteral("screen") : QStringLiteral("desktop");
}

BoundsMode boundsModeFromName(const QString& s, BoundsMode fallback)
{
    const QString m = norm(s);
    if (m == QLatin1String("screen") || m == QLatin1String("full")
        || m == QLatin1String("geometry")) {
        return BoundsMode::Screen;
    }
    if (m == QLatin1String("desktop") || m == QLatin1String("available")
        || m == QLatin1String("workarea") || m == QLatin1String("work")) {
        return BoundsMode::Desktop;
    }
    return fallback;
}

QString layoutIdSuffix(const QString& id)
{
    if (id.endsWith(QLatin1String("_sym_shift"))) {
        return QStringLiteral("_sym_shift");
    }
    if (id.endsWith(QLatin1String("_shift"))) {
        return QStringLiteral("_shift");
    }
    if (id.endsWith(QLatin1String("_sym"))) {
        return QStringLiteral("_sym");
    }
    return {};
}

QString layoutFamilyId(const QString& id)
{
    const QString suffix = layoutIdSuffix(id);
    if (suffix.isEmpty()) {
        return id;
    }
    return id.left(id.size() - suffix.size());
}

QString editorPreviewId(const QString& layoutId)
{
    return QStringLiteral("__editor_preview") + layoutIdSuffix(layoutId);
}

} // namespace LayoutSchema
} // namespace gazer

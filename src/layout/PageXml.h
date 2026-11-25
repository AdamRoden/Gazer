#pragma once

#include "layout/PageTypes.h"

#include <QString>
#include <QStringView>
#include <array>
#include <utility>

namespace gazer {

struct ControlTag {
    const char* element;
    /// Role this element implies. Empty is a plain button.
    const char* role;
};

/// Element name, then the role it writes. `Cell` loads as a button and is not written.
inline constexpr std::array<ControlTag, 9> kControlTags{{
    {"Button", ""},
    {"Cell", ""},
    {"Label", "label"},
    {"Display", "display"},
    {"Slider", "slider"},
    {"Switch", "toggle"},
    {"Choice", "choice"},
    {"Tab", "tab"},
    {"Swatch", "swatch"},
}};

/// Roles that share an element with the canonical role and are still written as `role`.
inline constexpr std::array<std::pair<const char*, const char*>, 3> kSharedControlRoles{{
    {"value", "Label"},
    {"preview", "Display"},
    {"swatchrect", "Swatch"},
}};

[[nodiscard]] inline PageFlow pageFlowFromElement(QStringView name)
{
    if (name == QLatin1String("Row")) {
        return PageFlow::Row;
    }
    if (name == QLatin1String("Column")) {
        return PageFlow::Column;
    }
    if (name == QLatin1String("Card")) {
        return PageFlow::Card;
    }
    return PageFlow::Mesh;
}

[[nodiscard]] inline QString pageFlowElement(PageFlow flow)
{
    switch (flow) {
    case PageFlow::Row:
        return QStringLiteral("Row");
    case PageFlow::Column:
        return QStringLiteral("Column");
    case PageFlow::Card:
        return QStringLiteral("Card");
    case PageFlow::Mesh:
        break;
    }
    return QStringLiteral("Grid");
}

[[nodiscard]] inline bool isFlowElement(QStringView name)
{
    return name == QLatin1String("Grid") || name == QLatin1String("SubGrid")
           || name == QLatin1String("Row") || name == QLatin1String("Column")
           || name == QLatin1String("Card");
}

[[nodiscard]] inline bool isControlElement(QStringView name)
{
    for (const ControlTag& tag : kControlTags) {
        if (name == QLatin1String(tag.element)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline QString roleForControlElement(QStringView name)
{
    for (const ControlTag& tag : kControlTags) {
        if (name == QLatin1String(tag.element)) {
            return QString::fromLatin1(tag.role);
        }
    }
    return {};
}

[[nodiscard]] inline QString controlElementForRole(const QString& role)
{
    const QString key = role.trimmed().toLower();
    for (const auto& shared : kSharedControlRoles) {
        if (key == QLatin1String(shared.first)) {
            return QString::fromLatin1(shared.second);
        }
    }
    for (const ControlTag& tag : kControlTags) {
        if (tag.role[0] != '\0' && key == QLatin1String(tag.role)) {
            return QLatin1String(tag.element);
        }
    }
    return QStringLiteral("Button");
}

} // namespace gazer

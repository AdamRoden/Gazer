#pragma once

#include "layout/PageTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QXmlStreamAttributes>

namespace gazer {

[[nodiscard]] bool parsePageAction(const QXmlStreamAttributes& attrs, const QString& cdata,
                                   PageAction& out, QString* error = nullptr);
[[nodiscard]] bool parsePageActionElement(QStringView elementName,
                                          const QXmlStreamAttributes& attrs, const QString& cdata,
                                          PageAction& out, QString* error = nullptr);
[[nodiscard]] bool parsePageActionAttribute(QStringView name, QStringView value, PageAction& out,
                                            QString* error = nullptr);
[[nodiscard]] bool takePageActionAttributes(const QXmlStreamAttributes& attrs,
                                            QVector<PageAction>& actions, QString* error = nullptr);

[[nodiscard]] bool isPageActionElementName(QStringView name);
[[nodiscard]] QString pageActionAttributeName(const PageAction& a);
[[nodiscard]] QString pageActionElementName(const PageAction& a);
[[nodiscard]] QString pageActionValueText(const PageAction& a);
[[nodiscard]] bool pageActionCanInline(const PageAction& a);

/// Inspector / XML spell: "Send", "OpenPage", "GoBack", …
[[nodiscard]] QString pageActionSpell(const PageAction& a);
[[nodiscard]] bool applyPageActionSpell(PageAction& a, const QString& spell);
[[nodiscard]] QStringList pageActionSpells();

[[nodiscard]] QString pageActionClickKindText(PageClickKind k);
[[nodiscard]] QStringList pageActionClickKindChoices();
[[nodiscard]] bool applyPageActionClickKind(PageAction& a, const QString& token,
                                            QString* error = nullptr);

[[nodiscard]] QString pageActionZoomText(const PageAction& a);
[[nodiscard]] QStringList pageActionZoomChoices();
[[nodiscard]] bool applyPageActionZoom(PageAction& a, const QString& token,
                                       QString* error = nullptr);

[[nodiscard]] QString pageActionCompassToken(PageAnchor a);

[[nodiscard]] QString pageRunKindText(PageRunKind k);
[[nodiscard]] QStringList pageRunKindChoices();
[[nodiscard]] bool applyPageRunKind(PageAction& a, const QString& token, QString* error = nullptr);

} // namespace gazer

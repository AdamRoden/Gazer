#pragma once

#include "layout/PageTypes.h"

#include <QXmlStreamAttributes>

namespace gazer {

[[nodiscard]] bool parsePageAction(const QXmlStreamAttributes& attrs, const QString& cdata,
                                   PageAction& out, QString* error = nullptr);

} // namespace gazer

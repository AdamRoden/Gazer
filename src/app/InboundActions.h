#pragma once

#include "layout/PageTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace gazer {

/// Concatenate every `--action` / `--action=` argument, newline-separated.
[[nodiscard]] QString inboundPayloadFromArgs(const QStringList& args);

/// Payload for a second process: `--action` lines, plus `command=openPageEditor` when
/// `--editor` is set. No `--action` and no `--editor` is the `raise` token.
[[nodiscard]] QString inboundForwardPayload(const QStringList& args);

[[nodiscard]] bool isInboundRaise(const QString& text);

/// Page XML action language: attribute lines (`openPage=qwerty_main`) and/or action
/// elements (`<ShowLayers value="2"/>`). Empty text is zero actions.
[[nodiscard]] bool parseInboundActions(const QString& text, QVector<PageAction>& out,
                                       QString* error = nullptr);

} // namespace gazer

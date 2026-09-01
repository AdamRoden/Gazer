#pragma once

#include "layout/PageTypes.h"

#include <QString>
#include <QStringList>

namespace gazer {
namespace pageaction {

[[nodiscard]] QStringList splitCsv(const QString& value);
[[nodiscard]] QString csvJoin(const QStringList& parts);
[[nodiscard]] QString buttonKey(const QString& button);

[[nodiscard]] QString clickKindText(PageClickKind k);
[[nodiscard]] QString zoomSpecText(const PageAction& a);
[[nodiscard]] QString compassToken(PageAnchor a);
[[nodiscard]] QString navTargetText(const PageAction& a);

bool parseClickKindToken(const QString& tok, PageAction& out, QString* error);
bool parseZoomSpec(const QString& tok, PageAction& out, QString* error);

bool parseSendValue(const QString& value, PageAction& out, QString* error);
bool parseCommandValue(const QString& value, PageAction& out, QString* error);
bool parseSpeakValue(const QString& value, PageAction& out, QString* error);
bool parseNavValue(const QString& value, PageAction& out, QString* error);
bool parseLayersValue(const QString& value, PageAction& out, QString* error);
bool parseClickKindValue(const QString& value, PageAction& out, QString* error);
bool parseGazeMove(const QString& value, PageAction& out, QString* error);
bool parseGazeClick(const QString& value, PageAction& out, QString* error);
bool parseMoveDir(const QString& value, PageAction& out, QString* error);
bool parseMovePoint(const QString& value, PageAction& out, QString* error);
bool parseLegacyClick(const QString& value, PageAction& out, QString* error);
bool parseLegacyMove(const QString& value, PageAction& out, QString* error);
bool parseLegacyMoveAndClick(const QString& value, PageAction& out, QString* error);
bool parseLegacyPageParts(const QStringList& parts, PageAction& out, QString* error);

} // namespace pageaction
} // namespace gazer

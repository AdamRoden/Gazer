#pragma once

#include "layout/PageHit.h"

#include <QChar>
#include <QHash>
#include <QRectF>
#include <QString>
#include <QVector>

namespace gazer {

/// Symbol a rapid character cell types. Null for Shift, Backspace, Enter, and the rest.
[[nodiscard]] QChar characterSymbol(const PageTarget& target);

struct KeyGravityCell {
    QString id;
    QString gridId;
    QRectF rect;
    /// Null when the cell is not a character key.
    QChar symbol;
};

struct KeyGravityQuery {
    QVector<KeyGravityCell> cells;
    /// Index of the geometric hit. -1 when gaze is not on a cell.
    int geometricIndex = -1;
    QPointF gaze;
    /// 0 keeps the geometric hit. 100 is the full capture band.
    int strength = 0;
    const QHash<QChar, double>* probability = nullptr;
    /// Session id of the cell already dwelling, if any.
    QString stickyId;
};

/// Index into `cells`, or `geometricIndex` when gravity does not move the hit.
/// A gap (`geometricIndex < 0`) stays a miss. The inner half of a key never moves.
[[nodiscard]] int keyGravityPick(const KeyGravityQuery& query);

/// Multiplier for the settings rapid sequence. Authored per-cell activation is not scaled;
/// pass it back unchanged from `chooseRapidSteps`.
[[nodiscard]] double rapidDwellScale(int strength, double surprise);

/// `authored` non-empty wins. Otherwise scale `settingsRapid` by `rapidDwellScale`.
[[nodiscard]] QVector<int> chooseRapidSteps(const QVector<int>& settingsRapid,
                                            const QVector<int>* authored, int strength,
                                            double surprise);

} // namespace gazer

#include "app/ComposeUi.h"

#include "app/ComposeUiInternal.h"
#include "ui/BoardPaint.h"

#include <QFontMetrics>

#include <algorithm>
#include <cmath>

namespace gazer {

using compose_detail::commandAction;

namespace {

int predictionWidth(const QString& label)
{
    const QFont font(BoardPaint::segoeFamily(), 16, QFont::DemiBold);
    const int adv = int(std::ceil(QFontMetricsF(font).horizontalAdvance(label)));
    return std::clamp(adv + 32, 72, 280);
}

} // namespace

void ComposeUi::layoutPredictRow(PageDocument& doc) const
{
    PageGrid* predict = doc.findGrid(QStringLiteral("predict"));
    PageGrid* phrase = doc.findGrid(QStringLiteral("phrase"));
    const PageGrid* authPredict = m_composeAuthored.findGrid(QStringLiteral("predict"));
    const PageGrid* authPhrase = m_composeAuthored.findGrid(QStringLiteral("phrase"));
    if (!predict || !phrase || !authPredict || !authPhrase) {
        return;
    }
    const int pRow = authPredict->row;
    const int phRow = authPhrase->row;
    const int phSpan = std::max(1, authPhrase->rowSpan);
    const bool suggestions = !capturingPhrase() && m_settings.composePredictions;
    predict->row = pRow;
    predict->rowSpan = std::max(1, authPredict->rowSpan);
    predict->col = authPredict->col;
    predict->colSpan = std::max(1, authPredict->colSpan);
    phrase->row = phRow;
    phrase->rowSpan = phSpan;
    predict->rows = 1;
    predict->cells.clear();
    predict->columnTracks.clear();

    auto addButton = [&](const QString& id, const QString& icon, const QString& label,
                         const QString& command) {
        PageCell cell;
        cell.id = id;
        cell.icon = icon;
        cell.label = label;
        cell.row = 0;
        cell.col = predict->columnTracks.size();
        cell.actions.push_back(commandAction(command));
        predict->cells.push_back(std::move(cell));
        predict->columnTracks.push_back(PageTrackSize::fromDim(PageDim::pixels(72)));
    };

    addButton(QStringLiteral("pred_undo"), QStringLiteral("undo"), QString(),
              QStringLiteral("compose.undo"));
    addButton(QStringLiteral("pred_redo"), QStringLiteral("redo"), QString(),
              QStringLiteral("compose.redo"));
    predict->columnTracks.push_back(PageTrackSize::starWeight(1));
    if (suggestions) {
        const int wordCol = predict->columnTracks.size();
        for (int i = 0; i < m_shown.size(); ++i) {
            PageCell cell;
            cell.id = QStringLiteral("pred_%1").arg(i);
            cell.label = m_shown[i].label;
            cell.row = 0;
            cell.col = wordCol + i;
            cell.actions.push_back(
                commandAction(QStringLiteral("compose.acceptPrediction.%1").arg(i)));
            predict->cells.push_back(std::move(cell));
            predict->columnTracks.push_back(
                PageTrackSize::fromDim(PageDim::pixels(predictionWidth(m_shown[i].label))));
        }
        predict->columnTracks.push_back(PageTrackSize::starWeight(1));
    }
    addButton(QStringLiteral("pred_home"), QStringLiteral("alignLeft"), QString(),
              QStringLiteral("home"));
    addButton(QStringLiteral("pred_left"), QStringLiteral("arrowWest"), QString(),
              QStringLiteral("arrowLeft"));
    addButton(QStringLiteral("pred_right"), QStringLiteral("arrowEast"), QString(),
              QStringLiteral("arrowRight"));
    addButton(QStringLiteral("pred_end"), QStringLiteral("alignRight"), QString(),
              QStringLiteral("end"));
    predict->columns = predict->columnTracks.size();
}

} // namespace gazer

#include "app/SettingsUi.h"
#include "app/CommandRegistry.h"
#include "app/SettingsPageBuild.h"
#include "assist/HeadPoseMapper.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "mapping/HeadPoseCurve.h"
#include "ui/PageHostWindow.h"
#include "ui/PoseChart.h"

#include <QStringList>
#include <QUrl>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::makeNested;

namespace {

constexpr HeadPoseAxis kAxes[] = {HeadPoseAxis::Yaw,   HeadPoseAxis::Pitch, HeadPoseAxis::Roll,
                                  HeadPoseAxis::X,     HeadPoseAxis::Y,     HeadPoseAxis::Z};
constexpr HeadPoseDest kDests[] = {
    HeadPoseDest::MouseX, HeadPoseDest::MouseY, HeadPoseDest::ScrollV, HeadPoseDest::ScrollH,
    HeadPoseDest::GazeX,  HeadPoseDest::GazeY,  HeadPoseDest::JoyLX,   HeadPoseDest::JoyLY,
    HeadPoseDest::JoyRX,  HeadPoseDest::JoyRY,  HeadPoseDest::Command};

QString curveCaption(const QVector<HeadPoseCurvePoint>& pts)
{
    QStringList parts;
    for (const HeadPoseCurvePoint& p : pts) {
        parts << QStringLiteral("%1,%2").arg(p.in, 0, 'f', 2).arg(p.out, 0, 'f', 2);
    }
    return parts.join(QLatin1Char(';'));
}

QString encodeCmd(const QString& name)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(name));
}

QString decodeCmd(const QString& enc)
{
    return QString::fromUtf8(QByteArray::fromPercentEncoding(enc.toLatin1()));
}

QString destChipLabel(HeadPoseDest d)
{
    switch (d) {
    case HeadPoseDest::MouseX:
        return QStringLiteral("Mouse H");
    case HeadPoseDest::MouseY:
        return QStringLiteral("Mouse V");
    case HeadPoseDest::ScrollV:
        return QStringLiteral("Scroll V");
    case HeadPoseDest::ScrollH:
        return QStringLiteral("Scroll H");
    case HeadPoseDest::GazeX:
        return QStringLiteral("Gaze H");
    case HeadPoseDest::GazeY:
        return QStringLiteral("Gaze V");
    case HeadPoseDest::JoyLX:
        return QStringLiteral("Left X");
    case HeadPoseDest::JoyLY:
        return QStringLiteral("Left Y");
    case HeadPoseDest::JoyRX:
        return QStringLiteral("Right X");
    case HeadPoseDest::JoyRY:
        return QStringLiteral("Right Y");
    case HeadPoseDest::Command:
        return QStringLiteral("Command");
    }
    return QString::fromLatin1(headPoseDestLabel(d));
}

QString destChipIcon(HeadPoseDest d)
{
    switch (d) {
    case HeadPoseDest::MouseX:
    case HeadPoseDest::MouseY:
        return QStringLiteral("mouseMove");
    case HeadPoseDest::ScrollV:
        return QStringLiteral("ScrollUp");
    case HeadPoseDest::ScrollH:
        return QStringLiteral("ScrollLeft");
    case HeadPoseDest::GazeX:
    case HeadPoseDest::GazeY:
        return QStringLiteral("MouseMagneticCursor");
    case HeadPoseDest::JoyLX:
    case HeadPoseDest::JoyLY:
    case HeadPoseDest::JoyRX:
    case HeadPoseDest::JoyRY:
        return QStringLiteral("sportsEsports");
    case HeadPoseDest::Command:
        return QStringLiteral("adsClick");
    }
    return {};
}

QString sourceUnit(HeadPoseAxis a)
{
    switch (a) {
    case HeadPoseAxis::X:
    case HeadPoseAxis::Y:
    case HeadPoseAxis::Z:
        return QStringLiteral("cm");
    default:
        return QString(QChar(0x00B0));
    }
}

QString destUnit(HeadPoseDest d)
{
    switch (d) {
    case HeadPoseDest::MouseX:
    case HeadPoseDest::MouseY:
        return QStringLiteral("px/s");
    case HeadPoseDest::ScrollV:
    case HeadPoseDest::ScrollH:
        return QStringLiteral("n/s");
    case HeadPoseDest::GazeX:
    case HeadPoseDest::GazeY:
        return QStringLiteral("px");
    default:
        return {};
    }
}

QString formatAxisValue(double v, const QString& unit)
{
    const QString n = QString::number(v, 'f', qAbs(v) >= 100.0 ? 0 : 1);
    return unit.isEmpty() ? n : n + unit;
}

QString mapStatusCaption(const HeadPoseMap& m)
{
    QString caption = m.dest == HeadPoseDest::Command ? headPoseMapDestSummary(m)
                                                      : destChipLabel(m.dest);
    if (!m.enabled) {
        caption += QStringLiteral(" · off");
    }
    return caption;
}

void roundCell(PageCell& c, double radius = 8.0)
{
    c.styleId = QStringLiteral("plain");
    c.style.thickness = PageBox::all(0.0);
    c.style.radius = PageBox::all(radius);
}

PageGrid rowGrid(const QString& id, int row, int cols, int gap)
{
    PageGrid g = makeNested(id, row, 0, 1, cols, gap);
    g.styleId = QStringLiteral("row");
    return g;
}

int pointIndexForInput(const QVector<HeadPoseCurvePoint>& pts, double inValue, int fallback)
{
    for (int i = 0; i < pts.size(); ++i) {
        if (qAbs(pts[i].in - inValue) < 1e-4) {
            return i;
        }
    }
    if (pts.isEmpty()) {
        return 0;
    }
    return qBound(0, fallback, pts.size() - 1);
}

constexpr auto kHeadPosePageId = QLatin1String("main_settings_head_pose");

} // namespace

void SettingsUi::decorateHeadPosePage(PageDocument& doc)
{
    if (!m_headPoseSeen) {
        discardHeadPoseEditor();
        m_headPoseSeen = true;
    }
    if (!m_headMapId.isEmpty() && !headPoseDraft()) {
        m_headMapId.clear();
        m_headColumn = HeadPoseColumn::Axes;
        endCurveScrub();
    }
    if (!m_headChartMapId.isEmpty()) {
        bool found = false;
        for (const HeadPoseMap& m : m_settings.headPoseMaps) {
            if (m.id == m_headChartMapId) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_headChartMapId.clear();
        }
    }

    PageCell* rec = PageEdit::findCell(doc, QStringLiteral("hp_recenter"));
    if (rec) {
        rec->caption = m_settings.headPoseOriginSet
                           ? QStringLiteral("Zeroed yaw, pitch, roll, x, y, z")
                           : QStringLiteral("Zero all six axes at current pose");
    }
    PageCell* curve = PageEdit::findCell(doc, QStringLiteral("pose_curve"));
    if (curve) {
        const bool editing = headPoseEditorOpen();
        const HeadPoseMap* shown = editing ? headPoseDraft() : mapForChartAxis();
        const QVector<HeadPoseCurvePoint> pts =
            shown ? shown->points : defaultHeadPoseMap().points;
        const HeadPoseAxis axis = shown ? shown->source : m_headChartAxis;
        curve->caption = curveCaption(pts);
        curve->label = QStringLiteral("%1 in → out").arg(QLatin1String(headPoseAxisLabel(axis)));
    }
    if (PageGrid* maps = PageEdit::findGrid(doc, QStringLiteral("sec_maps"))) {
        fillHeadPoseColumn(*maps);
    }
}

HeadPoseMap* SettingsUi::mapForChartAxis()
{
    if (!m_headChartMapId.isEmpty()) {
        for (HeadPoseMap& m : m_settings.headPoseMaps) {
            if (m.id == m_headChartMapId) {
                return &m;
            }
        }
    }
    for (HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.source == m_headChartAxis) {
            return &m;
        }
    }
    return nullptr;
}

const HeadPoseMap* SettingsUi::mapForChartAxis() const
{
    if (!m_headChartMapId.isEmpty()) {
        for (const HeadPoseMap& m : m_settings.headPoseMaps) {
            if (m.id == m_headChartMapId) {
                return &m;
            }
        }
    }
    for (const HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.source == m_headChartAxis) {
            return &m;
        }
    }
    return nullptr;
}

QString SettingsUi::headMapSourceId() const
{
    const HeadPoseMap* m = headPoseDraft();
    return m ? QLatin1String(headPoseAxisId(m->source)) : QString();
}

QString SettingsUi::headMapDestId() const
{
    const HeadPoseMap* m = headPoseDraft();
    return m ? QLatin1String(headPoseDestId(m->dest)) : QString();
}

QString SettingsUi::headMapCommand() const
{
    const HeadPoseMap* m = headPoseDraft();
    return m ? m->command : QString();
}

bool SettingsUi::headMapEnabled() const
{
    const HeadPoseMap* m = headPoseDraft();
    return m && m->enabled;
}

void SettingsUi::setHeadChartAxis(HeadPoseAxis axis)
{
    m_headChartMapId.clear();
    m_headChartAxis = axis;
    m_pages.refreshDecorated();
    m_pages.refreshActive();
    syncHeadPosePaint();
}

bool SettingsUi::selectHeadChartMap(const QString& mapId)
{
    for (const HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.id != mapId) {
            continue;
        }
        m_headChartAxis = m.source;
        m_headChartMapId = m.id;
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        syncHeadPosePaint();
        return true;
    }
    return false;
}

void SettingsUi::fillHeadPoseColumn(PageGrid& host)
{
    host.cells.clear();
    host.subGrids.clear();
    host.columns = 1;
    host.columnTracks.clear();
    host.gapPx = 6;
    if (!headPoseEditorOpen() || !headPoseDraft()) {
        fillHeadPoseAxisList(host);
        return;
    }
    switch (m_headColumn) {
    case HeadPoseColumn::Outputs:
        fillHeadPoseOutputList(host);
        break;
    case HeadPoseColumn::Commands:
        fillHeadPoseCommandColumn(host);
        break;
    case HeadPoseColumn::Axes:
    case HeadPoseColumn::Edit:
        fillHeadPoseEditorColumn(host);
        break;
    }
}

void SettingsUi::fillHeadPoseAxisList(PageGrid& host)
{
    struct Row {
        HeadPoseAxis axis = HeadPoseAxis::Yaw;
        bool mapped = false;
        QString id;
        QString caption;
    };
    QVector<Row> rows;
    for (const HeadPoseAxis axis : kAxes) {
        bool any = false;
        for (const HeadPoseMap& m : m_settings.headPoseMaps) {
            if (m.source != axis) {
                continue;
            }
            Row row;
            row.axis = axis;
            row.mapped = true;
            row.id = m.id;
            row.caption = mapStatusCaption(m);
            rows.push_back(row);
            any = true;
        }
        if (!any) {
            Row row;
            row.axis = axis;
            row.id = QLatin1String(headPoseAxisId(axis));
            row.caption = QStringLiteral("Not mapped");
            rows.push_back(row);
        }
    }

    host.rows = rows.size();
    host.rowTracks.clear();
    const EditorSwatch sw = editorSwatch();
    for (int i = 0; i < rows.size(); ++i) {
        const Row& row = rows[i];
        PageGrid nest = rowGrid(QStringLiteral("ax_%1").arg(row.id), i, 3, 6);
        nest.columnTracks = starTracks({4.2, 1.45, 1.45});
        const QString axisName = QString::fromLatin1(headPoseAxisLabel(row.axis));
        if (!row.mapped) {
            const QString axisCmd = QStringLiteral("headPose.chart.axis.%1").arg(row.id);
            PageCell name = cell(QStringLiteral("name_%1").arg(row.id), axisName, 0, 0, axisCmd,
                                 QColor(), 1, QStringLiteral("choice"), row.caption);
            name.activeState = axisCmd;
            roundCell(name, 10.0);
            nest.cells.push_back(name);
            PageCell add = cell(QStringLiteral("add_%1").arg(row.id), QStringLiteral("Add"), 0, 1,
                                QStringLiteral("headPose.axis.%1.add").arg(row.id), sw.add, 2, {},
                                {}, QStringLiteral("add"));
            roundCell(add, 10.0);
            nest.cells.push_back(add);
        } else {
            const QString mapCmd = QStringLiteral("headPose.chart.map.%1").arg(row.id);
            PageCell name = cell(QStringLiteral("name_%1").arg(row.id), axisName, 0, 0, mapCmd,
                                 QColor(), 1, QStringLiteral("choice"), row.caption);
            name.activeState = mapCmd;
            roundCell(name, 10.0);
            nest.cells.push_back(name);
            PageCell edit =
                cell(QStringLiteral("edit_%1").arg(row.id), QStringLiteral("Edit"), 0, 1,
                     QStringLiteral("headPose.edit.%1").arg(row.id), sw.edit, 1, {}, {},
                     QStringLiteral("editSquare"));
            roundCell(edit, 10.0);
            nest.cells.push_back(edit);
            PageCell dup =
                cell(QStringLiteral("dup_%1").arg(row.id), QStringLiteral("Duplicate"), 0, 2,
                     QStringLiteral("headPose.duplicate.%1").arg(row.id), sw.nudge, 1, {}, {},
                     QStringLiteral("contentCopy"));
            roundCell(dup, 10.0);
            nest.cells.push_back(dup);
        }
        host.subGrids.push_back(std::move(nest));
    }
}

void SettingsUi::fillHeadPoseEditorColumn(PageGrid& host)
{
    const HeadPoseMap* m = headPoseDraft();
    if (!m) {
        fillHeadPoseAxisList(host);
        return;
    }
    if (!m->points.isEmpty()) {
        m_headPointIndex = qBound(0, m_headPointIndex, m->points.size() - 1);
    } else {
        m_headPointIndex = 0;
    }
    const int n = m->points.size();
    host.rows = n + 3;
    QVector<double> weights;
    weights.reserve(n + 3);
    weights.push_back(1.15);
    weights.push_back(1.25);
    for (int i = 0; i < n; ++i) {
        weights.push_back(1.0);
    }
    weights.push_back(1.05);
    host.rowTracks = starTracks(weights);
    const EditorSwatch sw = editorSwatch();

    PageGrid act = rowGrid(QStringLiteral("act_row"), 0, 3, 6);
    PageCell en = cell(QStringLiteral("act_en"), QStringLiteral("Enable"), 0, 0,
                       QStringLiteral("headPose.map.enabled.toggle"), QColor(), 1,
                       QStringLiteral("toggle"));
    en.activeState = QStringLiteral("headPose.map.enabled");
    roundCell(en);
    act.cells.push_back(en);
    PageCell del = cell(QStringLiteral("act_del"), QStringLiteral("Delete"), 0, 1,
                        QStringLiteral("headPose.map.delete"), sw.cancel, 1, {}, {},
                        QStringLiteral("delete"));
    roundCell(del);
    act.cells.push_back(del);
    PageCell done = cell(QStringLiteral("act_done"), QStringLiteral("Done"), 0, 2,
                         QStringLiteral("headPose.map.done"), sw.save, 1, {}, {},
                         QStringLiteral("check"));
    roundCell(done);
    act.cells.push_back(done);
    host.subGrids.push_back(std::move(act));

    const auto invertButton = [&](PageGrid& out, int col) {
        PageCell invert = cell(QStringLiteral("out_invert"), QStringLiteral("Invert"), 0, col,
                               QStringLiteral("headPose.map.invert"), sw.nudge);
        roundCell(invert);
        out.cells.push_back(invert);
    };
    if (m->dest != HeadPoseDest::Command) {
        PageGrid out = rowGrid(QStringLiteral("out_row"), 1, 3, 6);
        out.columnTracks = starTracks({4.0, 1.45, 1.45});
        PageCell name = cell(QStringLiteral("out_name"), destChipLabel(m->dest), 0, 0, {}, QColor(),
                             1, QStringLiteral("label"), {}, destChipIcon(m->dest));
        name.textStyle = QStringLiteral("title");
        roundCell(name);
        out.cells.push_back(name);
        PageCell pick = cell(QStringLiteral("out_pick"), QStringLiteral("Output"), 0, 1,
                             QStringLiteral("headPose.map.pickOutput"), sw.edit);
        roundCell(pick);
        out.cells.push_back(pick);
        invertButton(out, 2);
        host.subGrids.push_back(std::move(out));
    } else {
        PageGrid out = rowGrid(QStringLiteral("out_row"), 1, 6, 4);
        out.columnTracks = starTracks({2.2, 0.75, 1.15, 0.75, 1.25, 1.25});
        const QString shown = m->command.isEmpty() ? QStringLiteral("Command") : m->command;
        const QString cap = m->command.isEmpty() ? QStringLiteral("Choose a command") : QString();
        PageCell name = cell(QStringLiteral("out_name"), shown, 0, 0,
                             QStringLiteral("headPose.map.pickCommand"), QColor(), 1, {}, cap);
        name.textStyle = QStringLiteral("title");
        roundCell(name);
        out.cells.push_back(name);
        PageCell decAt = cell(QStringLiteral("at_dec"), QStringLiteral("−"), 0, 1,
                              QStringLiteral("headPose.map.commandAt.dec"), sw.nudge);
        roundCell(decAt);
        out.cells.push_back(decAt);
        PageCell val = cell(QStringLiteral("at_val"),
                            formatAxisValue(m->commandAt, sourceUnit(m->source)), 0, 2, {}, sw.value,
                            1, QStringLiteral("value"), QStringLiteral("Trigger"));
        roundCell(val);
        out.cells.push_back(val);
        PageCell incAt = cell(QStringLiteral("at_inc"), QStringLiteral("+"), 0, 3,
                              QStringLiteral("headPose.map.commandAt.inc"), sw.nudge);
        roundCell(incAt);
        out.cells.push_back(incAt);
        PageCell pick = cell(QStringLiteral("out_pick"), QStringLiteral("Output"), 0, 4,
                             QStringLiteral("headPose.map.pickOutput"), sw.edit);
        roundCell(pick);
        out.cells.push_back(pick);
        invertButton(out, 5);
        host.subGrids.push_back(std::move(out));
    }

    const QString inUnit = sourceUnit(m->source);
    const QString outUnit = destUnit(m->dest);
    for (int pointIndex = 0; pointIndex < n; ++pointIndex) {
        const HeadPoseCurvePoint& pt = m->points[pointIndex];
        PageGrid row = rowGrid(QStringLiteral("pt_%1").arg(pointIndex), pointIndex + 2, 3, 6);
        row.columnTracks = starTracks({4.2, 4.2, 0.9});
        const auto cluster = [&](const QString& id, int col, const QString& op, const QString& value,
                                 const QString& caption) {
            PageGrid g = makeNested(id, 0, col, 1, 4, 0);
            g.styleId = QStringLiteral("row");
            g.columnTracks = starTracks({0.8, 1.45, 0.8, 1.15});
            const QString base =
                QStringLiteral("headPose.map.point.%1.%2").arg(pointIndex).arg(op);
            const auto piece = [&](const QString& suffix, const QString& lab, int pieceCol,
                                   const QString& cmd, const QColor& bg, const char* style,
                                   const QString& role, const QString& cap, const QString& icon) {
                PageCell c = cell(id + suffix, lab, 0, pieceCol, cmd, bg, 1, role, cap, icon);
                c.styleId = QLatin1String(style);
                c.style.thickness = PageBox::all(0.0);
                g.cells.push_back(std::move(c));
            };
            piece(QStringLiteral("_dec"), QStringLiteral("−"), 0,
                  base + QStringLiteral(".dec"), sw.nudge, "joinLeft", {}, {}, {});
            piece(QStringLiteral("_val"), value, 1, {}, sw.value, "join", QStringLiteral("value"),
                  caption, {});
            piece(QStringLiteral("_inc"), QStringLiteral("+"), 2,
                  base + QStringLiteral(".inc"), sw.nudge, "join", {}, {}, {});
            piece(QStringLiteral("_edit"), QStringLiteral("Edit"), 3,
                  base + QStringLiteral(".edit"), sw.edit, "joinRight", {}, {},
                  QStringLiteral("editSquare"));
            return g;
        };
        PageGrid inG = cluster(QStringLiteral("p%1_in").arg(pointIndex), 0, QStringLiteral("in"),
                               formatAxisValue(pt.in, inUnit), QStringLiteral("Input"));
        PageGrid outG = cluster(QStringLiteral("p%1_out").arg(pointIndex), 1, QStringLiteral("out"),
                                formatAxisValue(pt.out, outUnit), QStringLiteral("Output"));
        row.subGrids.push_back(std::move(inG));
        row.subGrids.push_back(std::move(outG));
        PageCell trash = cell(QStringLiteral("p%1_del").arg(pointIndex), {}, 0, 2,
                              QStringLiteral("headPose.map.point.%1.del").arg(pointIndex), sw.cancel,
                              1, {}, {}, QStringLiteral("delete"));
        roundCell(trash);
        row.cells.push_back(trash);
        host.subGrids.push_back(std::move(row));
    }

    PageGrid addRow = rowGrid(QStringLiteral("add_row"), n + 2, 1, 0);
    PageCell add = cell(QStringLiteral("act_add"), QStringLiteral("Add"), 0, 0,
                        QStringLiteral("headPose.map.point.add"), sw.add, 1, {}, {},
                        QStringLiteral("add"));
    roundCell(add);
    addRow.cells.push_back(add);
    host.subGrids.push_back(std::move(addRow));
}

void SettingsUi::fillHeadPoseOutputList(PageGrid& host)
{
    host.rows = 12;
    host.rowTracks.clear();
    const EditorSwatch sw = editorSwatch();
    PageGrid title = rowGrid(QStringLiteral("out_head"), 0, 2, 6);
    title.columnTracks = starTracks({4.2, 1.45});
    PageCell heading = cell(QStringLiteral("out_title"), QStringLiteral("Output"), 0, 0, {}, QColor(),
                            1, QStringLiteral("label"));
    heading.textStyle = QStringLiteral("section");
    roundCell(heading);
    title.cells.push_back(heading);
    PageCell back = cell(QStringLiteral("out_back"), QStringLiteral("Back"), 0, 1,
                         QStringLiteral("headPose.map.outputs.back"), sw.cancel, 1, {}, {},
                         QStringLiteral("ArrowLeft"));
    roundCell(back);
    title.cells.push_back(back);
    host.subGrids.push_back(std::move(title));

    for (int i = 0; i < int(sizeof(kDests) / sizeof(kDests[0])); ++i) {
        const QString id = QLatin1String(headPoseDestId(kDests[i]));
        const bool command = kDests[i] == HeadPoseDest::Command;
        const QString cmd = command ? QStringLiteral("headPose.map.pickCommand")
                                    : QStringLiteral("headPose.map.dest.%1").arg(id);
        PageCell c = cell(QStringLiteral("dst_%1").arg(id), destChipLabel(kDests[i]), i + 1, 0, cmd,
                          QColor(), 1, QStringLiteral("choice"));
        c.activeState = QStringLiteral("headPose.map.dest.%1").arg(id);
        roundCell(c);
        host.cells.push_back(c);
    }
}

void SettingsUi::fillHeadPoseCommandColumn(PageGrid& host)
{
    constexpr int kPage = 8;
    const QStringList names = headPoseCommandCatalog();
    const int pages = qMax(1, (names.size() + kPage - 1) / kPage);
    m_headCmdPage = qBound(0, m_headCmdPage, pages - 1);
    const int start = m_headCmdPage * kPage;
    const int pageRows = names.isEmpty() ? 1 : kPage;
    host.rows = pageRows + 1;
    host.rowTracks.clear();
    const EditorSwatch sw = editorSwatch();
    if (names.isEmpty()) {
        PageCell empty = cell(QStringLiteral("cmd_empty"), QStringLiteral("No commands"), 0, 0, {},
                              QColor(), 1, QStringLiteral("label"));
        roundCell(empty);
        host.cells.push_back(empty);
    } else {
        for (int i = 0; i < kPage; ++i) {
            const int idx = start + i;
            if (idx >= names.size()) {
                PageCell blank = cell(QStringLiteral("cmd_%1").arg(i), {}, i, 0, {}, QColor(), 1,
                                      QStringLiteral("label"));
                roundCell(blank);
                host.cells.push_back(blank);
                continue;
            }
            const QString enc = encodeCmd(names[idx]);
            PageCell c = cell(QStringLiteral("cmd_%1").arg(i), names[idx], i, 0,
                              QStringLiteral("headPose.cmd.%1").arg(enc), sw.nudge);
            c.activeState = QStringLiteral("headPose.map.command.%1").arg(enc);
            roundCell(c);
            host.cells.push_back(c);
        }
    }

    PageGrid nav = rowGrid(QStringLiteral("cmd_nav"), pageRows, 4, 6);
    nav.columnTracks = starTracks({1.4, 1.0, 1.4, 1.3});
    PageCell prev = cell(QStringLiteral("cmd_prev"), QStringLiteral("Previous"), 0, 0,
                         QStringLiteral("headPose.cmdList.prev"), sw.nudge, 1, {}, {},
                         QStringLiteral("ArrowLeft"));
    roundCell(prev);
    nav.cells.push_back(prev);
    PageCell pg = cell(QStringLiteral("cmd_pg"),
                       QStringLiteral("%1 / %2").arg(m_headCmdPage + 1).arg(pages), 0, 1, {},
                       sw.value, 1, QStringLiteral("value"));
    roundCell(pg);
    nav.cells.push_back(pg);
    PageCell next = cell(QStringLiteral("cmd_next"), QStringLiteral("Next"), 0, 2,
                         QStringLiteral("headPose.cmdList.next"), sw.nudge, 1, {}, {},
                         QStringLiteral("ArrowRight"));
    roundCell(next);
    nav.cells.push_back(next);
    PageCell back = cell(QStringLiteral("cmd_back"), QStringLiteral("Back"), 0, 3,
                         QStringLiteral("headPose.cmdList.cancel"), sw.cancel);
    roundCell(back);
    nav.cells.push_back(back);
    host.subGrids.push_back(std::move(nav));
}

void SettingsUi::registerHeadPoseCommands()
{
    m_commands.registerBuiltin(QStringLiteral("headPose.enabled.toggle"), [this](QString*) {
        m_settings.headPoseEnabled = !m_settings.headPoseEnabled;
        apply(true);
        notifyStatus(m_settings.headPoseEnabled ? QStringLiteral("Head pose maps on")
                                                : QStringLiteral("Head pose maps off"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.recenter"),
                               [this](QString* e) { return headPoseRecenter(e); });
    m_commands.registerBuiltin(QStringLiteral("headPose.addMap"),
                               [this](QString* e) { return headPoseAddMap(e); });
    m_commands.registerPrefix(
        QStringLiteral("headPose.chart.axis."),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            bool ok = false;
            const HeadPoseAxis axis = headPoseAxisFromId(
                inv.name.mid(int(QLatin1String("headPose.chart.axis.").size())), &ok);
            if (!ok) {
                return false;
            }
            setHeadChartAxis(axis);
            return true;
        });
    m_commands.registerPrefix(
        QStringLiteral("headPose.chart.map."),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            const QString id = inv.name.mid(int(QLatin1String("headPose.chart.map.").size()));
            return selectHeadChartMap(id);
        });
    m_commands.registerPrefix(
        QStringLiteral("headPose.axis."),
        [this](const CommandRegistry::Invocation& inv, QString* e) {
            const QString rest = inv.name.mid(int(QLatin1String("headPose.axis.").size()));
            if (!rest.endsWith(QLatin1String(".add"))) {
                return false;
            }
            bool ok = false;
            const HeadPoseAxis axis = headPoseAxisFromId(rest.left(rest.size() - 4), &ok);
            if (!ok) {
                return false;
            }
            return headPoseAddAxis(axis, e);
        });
    m_commands.registerPrefix(
        QStringLiteral("headPose.duplicate."),
        [this](const CommandRegistry::Invocation& inv, QString* e) {
            const QString id = inv.name.mid(int(QLatin1String("headPose.duplicate.").size()));
            if (id.isEmpty()) {
                return false;
            }
            return headPoseDuplicate(id, e);
        });
    m_commands.registerPrefix(QStringLiteral("headPose.edit."),
                              [this](const CommandRegistry::Invocation& inv, QString* e) {
                                  const QString id =
                                      inv.name.mid(int(QLatin1String("headPose.edit.").size()));
                                  return openHeadPoseEditor(id, e);
                              });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.enabled.toggle"), [this](QString*) {
        if (HeadPoseMap* m = headPoseDraft()) {
            m->enabled = !m->enabled;
            const bool on = m->enabled;
            commitHeadPoseDraft(*m, true);
            notifyStatus(on ? QStringLiteral("Map on") : QStringLiteral("Map off"));
            syncHeadPosePaint();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.delete"), [this](QString*) {
        const HeadPoseMap* draft = headPoseDraft();
        if (!draft) {
            return true;
        }
        const QString id = draft->id;
        const HeadPoseAxis axis = draft->source;
        discardHeadPoseEditor();
        if (m_headChartMapId == id) {
            m_headChartMapId.clear();
        }
        m_headChartAxis = axis;
        if (m_mutate) {
            m_mutate(
                [id](AppSettings& s) {
                    for (int i = 0; i < s.headPoseMaps.size(); ++i) {
                        if (s.headPoseMaps[i].id == id) {
                            s.headPoseMaps.removeAt(i);
                            break;
                        }
                    }
                },
                QStringLiteral("Map removed"));
        }
        syncHeadPosePaint();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.done"), [this](QString*) {
        closeHeadPoseEditor();
        apply(true);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.invert"),
                               [this](QString*) { return invertHeadPoseMap(); });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.pickOutput"), [this](QString*) {
        if (!headPoseEditorOpen()) {
            return false;
        }
        m_headColumn = HeadPoseColumn::Outputs;
        refreshHeadPoseEditor();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.outputs.back"), [this](QString*) {
        if (!headPoseEditorOpen()) {
            return false;
        }
        m_headColumn = HeadPoseColumn::Edit;
        refreshHeadPoseEditor();
        return true;
    });
    m_commands.registerPrefix(
        QStringLiteral("headPose.map.source."),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            bool ok = false;
            const HeadPoseAxis axis = headPoseAxisFromId(
                inv.name.mid(int(QLatin1String("headPose.map.source.").size())), &ok);
            if (!ok) {
                return false;
            }
            HeadPoseMap* m = headPoseDraft();
            if (!m) {
                return true;
            }
            m->source = axis;
            m_headChartAxis = axis;
            m_headChartMapId = m->id;
            m_headColumn = HeadPoseColumn::Edit;
            commitHeadPoseDraft(*m, true);
            syncHeadPosePaint();
            return true;
        });
    m_commands.registerPrefix(
        QStringLiteral("headPose.map.dest."),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            bool ok = false;
            const HeadPoseDest dest = headPoseDestFromId(
                inv.name.mid(int(QLatin1String("headPose.map.dest.").size())), &ok);
            if (!ok) {
                return false;
            }
            HeadPoseMap* m = headPoseDraft();
            if (!m) {
                return true;
            }
            m->dest = dest;
            m_headColumn = HeadPoseColumn::Edit;
            commitHeadPoseDraft(*m, true);
            syncHeadPosePaint();
            return true;
        });

    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.prev"), [this](QString*) {
        if (const HeadPoseMap* m = headPoseDraft()) {
            if (!m->points.isEmpty()) {
                m_headPointIndex = qBound(0, m_headPointIndex - 1, m->points.size() - 1);
                refreshHeadPoseEditor();
            }
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.next"), [this](QString*) {
        if (const HeadPoseMap* m = headPoseDraft()) {
            if (!m->points.isEmpty()) {
                m_headPointIndex = qBound(0, m_headPointIndex + 1, m->points.size() - 1);
                refreshHeadPoseEditor();
            }
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.add"),
                               [this](QString*) { return addHeadPosePoint(); });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.del"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("del"), e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.in.dec"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("in.dec"), e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.in.inc"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("in.inc"), e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.out.dec"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("out.dec"), e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.out.inc"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("out.inc"), e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.in.edit"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("in.edit"), e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.out.edit"), [this](QString* e) {
        return adjustHeadPosePoint(m_headPointIndex, QStringLiteral("out.edit"), e);
    });
    // Indexed form is "N.op". Unindexed names are exact builtins and never reach this prefix.
    m_commands.registerPrefix(
        QStringLiteral("headPose.map.point."),
        [this](const CommandRegistry::Invocation& inv, QString* e) {
            const QString rest = inv.name.mid(int(QLatin1String("headPose.map.point.").size()));
            const int dot = rest.indexOf(QLatin1Char('.'));
            if (dot <= 0) {
                return false;
            }
            bool ok = false;
            const int index = rest.left(dot).toInt(&ok);
            if (!ok) {
                return false;
            }
            return adjustHeadPosePoint(index, rest.mid(dot + 1), e);
        });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.curve.scrub"), [this](QString*) {
        beginCurveScrub();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.pickCommand"),
                               [this](QString* e) { return openHeadPoseCommandList(e); });
    m_commands.registerBuiltin(QStringLiteral("headPose.cmdList.next"), [this](QString*) {
        ++m_headCmdPage;
        refreshHeadPoseCommandList();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.cmdList.prev"), [this](QString*) {
        m_headCmdPage = qMax(0, m_headCmdPage - 1);
        refreshHeadPoseCommandList();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.cmdList.cancel"), [this](QString*) {
        if (!headPoseEditorOpen()) {
            m_headColumn = HeadPoseColumn::Axes;
            return true;
        }
        m_headColumn = HeadPoseColumn::Outputs;
        refreshHeadPoseEditor();
        return true;
    });
    m_commands.registerPrefix(QStringLiteral("headPose.cmd."),
                              [this](const CommandRegistry::Invocation& inv, QString*) {
                                  const QString enc =
                                      inv.name.mid(int(QLatin1String("headPose.cmd.").size()));
                                  if (enc.isEmpty()) {
                                      return false;
                                  }
                                  if (HeadPoseMap* m = headPoseDraft()) {
                                      m->command = decodeCmd(enc);
                                      m->dest = HeadPoseDest::Command;
                                      m_headColumn = HeadPoseColumn::Edit;
                                      commitHeadPoseDraft(*m, true);
                                      syncHeadPosePaint();
                                  }
                                  return true;
                              });
    auto nudgeAt = [this](int dir) {
        return [this, dir](QString*) {
            if (HeadPoseMap* m = headPoseDraft()) {
                m->commandAt = stepHeadPoseCommandAt(m->commandAt, dir);
                clampHeadPoseMap(*m);
                m_headColumn = HeadPoseColumn::Edit;
                commitHeadPoseDraft(*m, true);
                syncHeadPosePaint();
            }
            return true;
        };
    };
    m_commands.registerBuiltin(QStringLiteral("headPose.map.commandAt.dec"), nudgeAt(-1));
    m_commands.registerBuiltin(QStringLiteral("headPose.map.commandAt.inc"), nudgeAt(+1));
}

bool SettingsUi::headPoseRecenter(QString* error)
{
    if (!m_headPose || !m_headPose->lastPose().valid()) {
        const QString msg = QStringLiteral("No head pose to recenter");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const HeadPose pose = m_headPose->lastPose();
    if (m_mutate) {
        m_mutate(
            [pose](AppSettings& s) {
                captureHeadPoseOrigin(s.headPoseOrigin, pose);
                s.headPoseOriginSet = true;
            },
            QStringLiteral("Origin: yaw, pitch, roll, x, y, z"));
    }
    return true;
}

bool SettingsUi::headPoseAddMap(QString* error)
{
    return headPoseAddAxis(m_headChartAxis, error);
}

bool SettingsUi::headPoseAddAxis(HeadPoseAxis axis, QString* error)
{
    if (m_settings.headPoseMaps.size() >= kMaxHeadPoseMaps) {
        const QString msg = QStringLiteral("At most %1 maps").arg(kMaxHeadPoseMaps);
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    HeadPoseMap m = AppSettings::makeDefaultHeadPoseMap();
    m.source = axis;
    const QString id = m.id;
    if (m_mutate) {
        m_mutate([m](AppSettings& s) { s.headPoseMaps.push_back(m); }, QStringLiteral("Map added"));
    }
    return openHeadPoseEditor(id, error);
}

bool SettingsUi::headPoseDuplicate(const QString& mapId, QString* error)
{
    const HeadPoseMap* src = nullptr;
    for (const HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.id == mapId) {
            src = &m;
            break;
        }
    }
    if (!src) {
        const QString msg = QStringLiteral("Unknown map");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (m_settings.headPoseMaps.size() >= kMaxHeadPoseMaps) {
        const QString msg = QStringLiteral("At most %1 maps").arg(kMaxHeadPoseMaps);
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    HeadPoseMap copy = *src;
    copy.id = AppSettings::makeDefaultHeadPoseMap().id;
    const QString id = copy.id;
    if (m_mutate) {
        m_mutate([copy](AppSettings& s) { s.headPoseMaps.push_back(copy); },
                 QStringLiteral("Map copied"));
    }
    return openHeadPoseEditor(id, error);
}

HeadPoseMap* SettingsUi::headPoseDraft()
{
    for (HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.id == m_headMapId) {
            return &m;
        }
    }
    return nullptr;
}

const HeadPoseMap* SettingsUi::headPoseDraft() const
{
    for (const HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.id == m_headMapId) {
            return &m;
        }
    }
    return nullptr;
}

void SettingsUi::commitHeadPoseDraft(const HeadPoseMap& m, bool persist)
{
    for (HeadPoseMap& row : m_settings.headPoseMaps) {
        if (row.id == m.id) {
            row = m;
            break;
        }
    }
    m_settings.clamp();
    if (m_headPose) {
        m_headPose->setMaps(m_settings.headPoseMaps);
        m_headPose->setOrigin(m_settings.headPoseOrigin, m_settings.headPoseOriginSet);
        m_headPose->setEnabled(m_settings.headPoseEnabled);
    }
    if (persist) {
        apply(true);
    }
}

bool SettingsUi::openHeadPoseEditor(const QString& mapId, QString* error)
{
    const HeadPoseMap* found = nullptr;
    for (const HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.id == mapId) {
            found = &m;
            break;
        }
    }
    if (!found) {
        const QString msg = QStringLiteral("Unknown map");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    m_headMapId = mapId;
    m_headPointIndex = 0;
    m_headColumn = HeadPoseColumn::Edit;
    m_headChartAxis = found->source;
    m_headChartMapId = found->id;
    endCurveScrub();
    refreshHeadPoseEditor();
    return true;
}

void SettingsUi::discardHeadPoseEditor()
{
    endCurveScrub();
    m_headMapId.clear();
    m_headColumn = HeadPoseColumn::Axes;
    m_headPointIndex = 0;
    m_headCmdPage = 0;
}

void SettingsUi::onSessionChanged()
{
    if (m_pages.showsPage(QString(kHeadPosePageId))) {
        return;
    }
    m_headPoseSeen = false;
    if (m_headMapId.isEmpty() && m_headColumn == HeadPoseColumn::Axes) {
        return;
    }
    discardHeadPoseEditor();
    syncHeadPosePaint();
}

void SettingsUi::closeHeadPoseEditor()
{
    discardHeadPoseEditor();
    m_pages.refreshDecorated();
    m_pages.refreshActive();
    syncHeadPosePaint();
}

void SettingsUi::refreshHeadPoseEditor()
{
    if (!headPoseEditorOpen()) {
        return;
    }
    m_pages.refreshDecorated();
    m_pages.refreshActive();
    syncHeadPosePaint();
}

bool SettingsUi::adjustHeadPosePoint(int index, const QString& op, QString* error)
{
    HeadPoseMap* m = headPoseDraft();
    if (!m) {
        return false;
    }
    if (op == QLatin1String("in.edit") || op == QLatin1String("out.edit")) {
        if (index < 0 || index >= m->points.size()) {
            return false;
        }
        m_headPointIndex = index;
        m_headPointEditOut = op == QLatin1String("out.edit");
        m_numpadReturn = NumpadReturn::HeadPose;
        m_numpadTitle = m_headPointEditOut ? QStringLiteral("Output") : QStringLiteral("Input");
        m_numpadHint = m_headPointEditOut ? QStringLiteral("Mapped output")
                                          : QStringLiteral("Source value (deg or cm)");
        m_numpadBuffer = QString::number(m_headPointEditOut ? m->points[index].out
                                                            : m->points[index].in);
        m_numpadResetSeed = m_numpadBuffer;
        syncHeadPosePaint();
        return presentNumpad(error);
    }
    if (op == QLatin1String("del")) {
        if (index < 0 || index >= m->points.size()) {
            return false;
        }
        if (m->points.size() <= 2) {
            notifyStatus(QStringLiteral("Keep at least two points"));
            return true;
        }
        m->points.removeAt(index);
        clampHeadPoseMap(*m);
        m_headPointIndex = qBound(0, index, qMax(0, m->points.size() - 1));
        commitHeadPoseDraft(*m, true);
        syncHeadPosePaint();
        return true;
    }
    if (index < 0 || index >= m->points.size()) {
        return false;
    }
    HeadPoseCurvePoint& p = m->points[index];
    if (op == QLatin1String("in.dec")) {
        p.in -= 1.0;
    } else if (op == QLatin1String("in.inc")) {
        p.in += 1.0;
    } else if (op == QLatin1String("out.dec")) {
        p.out -= 10.0;
    } else if (op == QLatin1String("out.inc")) {
        p.out += 10.0;
    } else {
        return false;
    }
    const double editedIn = qBound(-1000.0, p.in, 1000.0);
    clampHeadPoseMap(*m);
    m_headPointIndex = pointIndexForInput(m->points, editedIn, index);
    commitHeadPoseDraft(*m, true);
    syncHeadPosePaint();
    return true;
}

bool SettingsUi::addHeadPosePoint()
{
    HeadPoseMap* m = headPoseDraft();
    if (!m) {
        return false;
    }
    if (m->points.size() >= kMaxHeadPoseCurvePoints) {
        notifyStatus(QStringLiteral("At most %1 points").arg(kMaxHeadPoseCurvePoints));
        return true;
    }
    HeadPoseCurvePoint p;
    p.in = m->points.isEmpty() ? 0.0 : m->points.last().in + 5.0;
    p.out = m->points.isEmpty() ? 0.0 : m->points.last().out;
    const double editedIn = qBound(-1000.0, p.in, 1000.0);
    m->points.push_back(p);
    clampHeadPoseMap(*m);
    m_headPointIndex = pointIndexForInput(m->points, editedIn, m->points.size() - 1);
    commitHeadPoseDraft(*m, true);
    syncHeadPosePaint();
    return true;
}

bool SettingsUi::invertHeadPoseMap()
{
    HeadPoseMap* m = headPoseDraft();
    if (!m) {
        return false;
    }
    for (HeadPoseCurvePoint& p : m->points) {
        p.out = -p.out;
    }
    clampHeadPoseMap(*m);
    commitHeadPoseDraft(*m, true);
    notifyStatus(QStringLiteral("Outputs inverted"));
    syncHeadPosePaint();
    return true;
}

QStringList SettingsUi::headPoseCommandCatalog() const
{
    QStringList names;
    for (const QString& n : m_commands.names()) {
        if (n.startsWith(QLatin1String("settings.")) || n.startsWith(QLatin1String("headPose."))
            || n.startsWith(QLatin1String("theme.")) || n.startsWith(QLatin1String("compose."))
            || n.startsWith(QLatin1String("speech.")) || n.startsWith(QLatin1String("soundboard."))
            || n.startsWith(QLatin1String("history.")) || n.startsWith(QLatin1String("gazer."))
            || n.startsWith(QLatin1String("lts.")) || n.startsWith(QLatin1String("lookTo."))) {
            continue;
        }
        names.push_back(n);
    }
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

bool SettingsUi::openHeadPoseCommandList(QString* error)
{
    if (!headPoseDraft()) {
        const QString msg = QStringLiteral("No map");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    m_headCmdPage = 0;
    m_headColumn = HeadPoseColumn::Commands;
    m_pages.refreshDecorated();
    m_pages.refreshActive();
    return true;
}

void SettingsUi::refreshHeadPoseCommandList()
{
    if (headPoseEditorOpen() && m_headColumn == HeadPoseColumn::Commands) {
        m_pages.refreshDecorated();
        m_pages.refreshActive();
    }
}

void SettingsUi::syncHeadPosePaint()
{
    PageHostWindow* w = m_pages.window();
    if (!w) {
        return;
    }
    if (headPoseEditorOpen()) {
        if (m_curveScrub) {
            return;
        }
        const HeadPoseMap* m = headPoseDraft();
        if (!m) {
            return;
        }
        const int selected = (m_headColumn == HeadPoseColumn::Edit && !m->points.isEmpty())
                                 ? qBound(0, m_headPointIndex, m->points.size() - 1)
                                 : -1;
        const double liveIn = m_headPose ? m_headPose->axisRelative(m->source) : 0.0;
        w->setCurve(m->points, selected, liveIn, m_headPose != nullptr);
        return;
    }
    const HeadPoseMap* chart = mapForChartAxis();
    const QVector<HeadPoseCurvePoint> pts =
        chart ? chart->points : defaultHeadPoseMap().points;
    const HeadPoseAxis axis = chart ? chart->source : m_headChartAxis;
    const double liveIn = m_headPose ? m_headPose->axisRelative(axis) : 0.0;
    w->setCurve(pts, -1, liveIn, m_headPose != nullptr);
}

void SettingsUi::beginCurveScrub()
{
    m_curveScrub = true;
    m_curveLeaveGrace.reset();
    m_curveLeaveGrace.graceMs = qMax(0, m_settings.dwellGraceMs);
    m_curveDwell.reset();
    m_curveDwell.setDwellMs(m_settings.mouseMoveDwellMs);
    m_curveScrubLastMs = -1;
    notifyStatus(QStringLiteral("Look on the curve to move the selected point"));
}

void SettingsUi::endCurveScrub()
{
    if (!m_curveScrub) {
        return;
    }
    m_curveScrub = false;
    m_curveDwell.reset();
    m_curveLeaveGrace.reset();
    m_curveScrubLastMs = -1;
}

void SettingsUi::feedCurveGaze(const GazePoint& point)
{
    if (!m_curveScrub) {
        return;
    }
    if (!headPoseEditorOpen()) {
        endCurveScrub();
        return;
    }
    const qint64 now = point.timestampMs;
    const QRect r = m_pages.targetScreenRect(QStringLiteral("main_settings_head_pose"),
                                             QStringLiteral("pose_curve"));
    const bool onCurve =
        point.valid && !r.isEmpty() && r.contains(point.toPointF().toPoint());
    if (!onCurve) {
        if (m_curveLeaveGrace.onInvalid(now) == InvalidGazeGrace::Result::Holding) {
            return;
        }
        endCurveScrub();
        refreshHeadPoseEditor();
        return;
    }
    m_curveLeaveGrace.onValid();
    HeadPoseMap* m = headPoseDraft();
    if (!m || m->points.isEmpty()) {
        endCurveScrub();
        refreshHeadPoseEditor();
        return;
    }
    if (m_headPointIndex < 0 || m_headPointIndex >= m->points.size()) {
        m_headPointIndex = 0;
    }
    double in = 0, out = 0;
    PoseChart::valueAt(QRectF(r), m->points, point.toPointF(), &in, &out);
    m->points[m_headPointIndex].in = in;
    m->points[m_headPointIndex].out = out;
    clampHeadPoseMap(*m);
    const double editedIn = qBound(-1000.0, in, 1000.0);
    m_headPointIndex = pointIndexForInput(m->points, editedIn, m_headPointIndex);
    commitHeadPoseDraft(*m, false);
    if (PageHostWindow* w = m_pages.window()) {
        w->setCurve(m->points, m_headPointIndex, in, true);
    }
    const double dtSec =
        m_curveScrubLastMs < 0
            ? 0.016
            : qBound(0.004, (now - m_curveScrubLastMs) / 1000.0, 0.08);
    m_curveScrubLastMs = now;
    if (m_curveDwell.sample(point.toPointF(), dtSec)) {
        notifyStatus(QStringLiteral("Point %1: %2 → %3")
                         .arg(m_headPointIndex + 1)
                         .arg(in, 0, 'f', 1)
                         .arg(out, 0, 'f', 1));
        endCurveScrub();
        refreshHeadPoseEditor();
    }
}

} // namespace gazer

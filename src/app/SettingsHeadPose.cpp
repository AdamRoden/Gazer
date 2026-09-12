#include "app/SettingsUi.h"
#include "app/CommandRegistry.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"
#include "assist/HeadPoseMapper.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "mapping/HeadPoseCurve.h"
#include "ui/PageHostWindow.h"
#include "ui/PoseChart.h"

#include <QUrl>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::adoptCallerBoard;
using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsPageBuild::makeNested;
using SettingsUiInternal::kLiveHeadPoseCmd;
using SettingsUiInternal::kLiveHeadPoseMap;

namespace {

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

void joinEnds(PageCell& c, int index, int count)
{
    c.style.thickness = PageBox::all(0.0);
    if (count <= 1) {
        c.style.radius = PageBox::all(10.0);
        return;
    }
    if (index == 0) {
        c.style.radius = PageBox::of(10.0, 0.0, 0.0, 10.0);
    } else if (index == count - 1) {
        c.style.radius = PageBox::of(0.0, 10.0, 10.0, 0.0);
    } else {
        c.style.radius = PageBox::all(0.0);
    }
}

PageCell sectionLabel(const QString& id, const QString& text, int row, int col, int span = 1)
{
    PageCell c = cell(id, text, row, col, {}, QColor(), span, QStringLiteral("label"));
    c.textStyle = QStringLiteral("section");
    return c;
}

} // namespace

void SettingsUi::decorateHeadPosePage(PageDocument& doc)
{
    PageCell* rec = PageEdit::findCell(doc, QStringLiteral("hp_recenter"));
    if (rec) {
        rec->caption = m_settings.headPoseOriginSet
                           ? QStringLiteral("Zeroed yaw, pitch, roll, x, y, z")
                           : QStringLiteral("Zero all six axes at current pose");
    }
    PageCell* curve = PageEdit::findCell(doc, QStringLiteral("pose_curve"));
    if (curve) {
        const HeadPoseMap* m = mapForChartAxis();
        const QVector<HeadPoseCurvePoint> pts =
            m ? m->points : defaultHeadPoseMap().points;
        curve->caption = curveCaption(pts);
        curve->label = QStringLiteral("%1 in → out")
                           .arg(QLatin1String(headPoseAxisLabel(m_headChartAxis)));
    }
    fillHeadPoseMaps(doc);
}

HeadPoseMap* SettingsUi::mapForChartAxis()
{
    for (HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.source == m_headChartAxis) {
            return &m;
        }
    }
    return nullptr;
}

const HeadPoseMap* SettingsUi::mapForChartAxis() const
{
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

bool SettingsUi::headMapEnabled() const
{
    const HeadPoseMap* m = headPoseDraft();
    return m && m->enabled;
}

void SettingsUi::setHeadChartAxis(HeadPoseAxis axis)
{
    m_headChartAxis = axis;
    m_pages.refreshDecorated();
    m_pages.refreshActive();
    notifyStatus(QStringLiteral("Graph input: %1").arg(QLatin1String(headPoseAxisLabel(axis))));
}

void SettingsUi::fillHeadPoseMaps(PageDocument& doc)
{
    PageGrid* maps = PageEdit::findGrid(doc, QStringLiteral("maps"));
    if (!maps) {
        return;
    }
    maps->cells.clear();
    maps->subGrids.clear();
    const QVector<HeadPoseMap>& list = m_settings.headPoseMaps;
    if (list.isEmpty()) {
        maps->rows = 1;
        maps->columns = 1;
        maps->cells.push_back(cell(QStringLiteral("maps_empty"), QStringLiteral("No maps yet"), 0, 0,
                                   {}, QColor(), 1, QStringLiteral("label"),
                                   QStringLiteral("Add a map to send this axis to the pointer, scroll, gaze, pad, or a command.")));
        return;
    }
    maps->rows = list.size();
    maps->columns = 4;
    maps->gapPx = 6;
    const EditorSwatch sw = editorSwatch();
    for (int i = 0; i < list.size(); ++i) {
        const HeadPoseMap& m = list[i];
        const QString summary =
            QStringLiteral("%1  →  %2")
                .arg(QLatin1String(headPoseAxisLabel(m.source)), destChipLabel(m.dest));
        PageCell name =
            cell(QStringLiteral("sum_%1").arg(m.id), summary, i, 0, {}, QColor(), 3,
                 QStringLiteral("label"),
                 m.enabled ? headPoseMapDestSummary(m) : QStringLiteral("Disabled"),
                 destChipIcon(m.dest));
        maps->cells.push_back(name);
        maps->cells.push_back(cell(QStringLiteral("edit_%1").arg(m.id), QStringLiteral("Edit"), i, 3,
                                   QStringLiteral("headPose.edit.%1").arg(m.id), sw.edit, 1, {}, {},
                                   QStringLiteral("editSquare")));
    }
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
    m_commands.registerPrefix(QStringLiteral("headPose.edit."),
                              [this](const CommandRegistry::Invocation& inv, QString* e) {
                                  const QString id =
                                      inv.name.mid(int(QLatin1String("headPose.edit.").size()));
                                  return openHeadPoseEditor(id, e);
                              });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.enabled.toggle"), [this](QString*) {
        if (HeadPoseMap* m = headPoseDraft()) {
            m->enabled = !m->enabled;
            commitHeadPoseDraft(*m);
            refreshHeadPoseEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.delete"), [this](QString*) {
        const QString id = m_headMapId;
        closeLive(m_headMap);
        m_headMapId.clear();
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
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.done"), [this](QString*) {
        closeLive(m_headMap);
        m_headMapId.clear();
        apply(true);
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
            if (HeadPoseMap* m = headPoseDraft()) {
                m->source = axis;
                commitHeadPoseDraft(*m, true);
                refreshHeadPoseEditor();
            }
            m_headChartAxis = axis;
            m_pages.refreshActive();
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
            if (HeadPoseMap* m = headPoseDraft()) {
                m->dest = dest;
                commitHeadPoseDraft(*m, true);
                refreshHeadPoseEditor();
            }
            return true;
        });

    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.prev"), [this](QString*) {
        if (const HeadPoseMap* m = headPoseDraft()) {
            m_headPointIndex = qBound(0, m_headPointIndex - 1, m->points.size() - 1);
            refreshHeadPoseEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.next"), [this](QString*) {
        if (const HeadPoseMap* m = headPoseDraft()) {
            m_headPointIndex = qBound(0, m_headPointIndex + 1, m->points.size() - 1);
            refreshHeadPoseEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.add"), [this](QString*) {
        if (HeadPoseMap* m = headPoseDraft()) {
            if (m->points.size() < kMaxHeadPoseCurvePoints) {
                HeadPoseCurvePoint p;
                p.in = m->points.isEmpty() ? 0.0 : m->points.last().in + 5.0;
                p.out = m->points.isEmpty() ? 0.0 : m->points.last().out;
                m->points.push_back(p);
                clampHeadPoseMap(*m);
                m_headPointIndex = m->points.size() - 1;
                commitHeadPoseDraft(*m, false);
                refreshHeadPoseEditor();
            }
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.del"), [this](QString*) {
        if (HeadPoseMap* m = headPoseDraft()) {
            if (m->points.size() > 2 && m_headPointIndex >= 0 && m_headPointIndex < m->points.size()) {
                m->points.removeAt(m_headPointIndex);
                clampHeadPoseMap(*m);
                m_headPointIndex = qBound(0, m_headPointIndex, m->points.size() - 1);
                commitHeadPoseDraft(*m, false);
                refreshHeadPoseEditor();
            }
        }
        return true;
    });
    auto nudgePoint = [this](bool outAxis, int dir) {
        return [this, outAxis, dir](QString*) {
            if (HeadPoseMap* m = headPoseDraft()) {
                if (m_headPointIndex < 0 || m_headPointIndex >= m->points.size()) {
                    return true;
                }
                HeadPoseCurvePoint& p = m->points[m_headPointIndex];
                const double step = outAxis ? 10.0 : 1.0;
                if (outAxis) {
                    p.out += dir * step;
                } else {
                    p.in += dir * step;
                }
                clampHeadPoseMap(*m);
                commitHeadPoseDraft(*m, false);
                refreshHeadPoseEditor();
            }
            return true;
        };
    };
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.in.dec"), nudgePoint(false, -1));
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.in.inc"), nudgePoint(false, +1));
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.out.dec"), nudgePoint(true, -1));
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.out.inc"), nudgePoint(true, +1));
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.in.edit"), [this](QString* e) {
        const HeadPoseMap* m = headPoseDraft();
        if (!m || m_headPointIndex < 0 || m_headPointIndex >= m->points.size()) {
            return false;
        }
        m_headPointEditOut = false;
        m_numpadReturn = NumpadReturn::HeadPose;
        m_numpadTitle = QStringLiteral("Input");
        m_numpadHint = QStringLiteral("Source value (deg or cm)");
        m_numpadBuffer = QString::number(m->points[m_headPointIndex].in);
        m_numpadResetSeed = m_numpadBuffer;
        return presentNumpad(e);
    });
    m_commands.registerBuiltin(QStringLiteral("headPose.map.point.out.edit"), [this](QString* e) {
        const HeadPoseMap* m = headPoseDraft();
        if (!m || m_headPointIndex < 0 || m_headPointIndex >= m->points.size()) {
            return false;
        }
        m_headPointEditOut = true;
        m_numpadReturn = NumpadReturn::HeadPose;
        m_numpadTitle = QStringLiteral("Output");
        m_numpadHint = QStringLiteral("Mapped output");
        m_numpadBuffer = QString::number(m->points[m_headPointIndex].out);
        m_numpadResetSeed = m_numpadBuffer;
        return presentNumpad(e);
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
        closeLive(m_headCmd);
        refreshHeadPoseEditor();
        return true;
    });
    m_commands.registerPrefix(QStringLiteral("headPose.cmd."),
                              [this](const CommandRegistry::Invocation& inv, QString*) {
                                  const QString enc =
                                      inv.name.mid(int(QLatin1String("headPose.cmd.").size()));
                                  if (HeadPoseMap* m = headPoseDraft()) {
                                      m->command = decodeCmd(enc);
                                      m->dest = HeadPoseDest::Command;
                                      commitHeadPoseDraft(*m);
                                  }
                                  closeLive(m_headCmd);
                                  refreshHeadPoseEditor();
                                  return true;
                              });
    auto nudgeAt = [this](int dir) {
        return [this, dir](QString*) {
            if (HeadPoseMap* m = headPoseDraft()) {
                m->commandAt += dir * 1.0;
                clampHeadPoseMap(*m);
                commitHeadPoseDraft(*m);
                refreshHeadPoseEditor();
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
    if (m_settings.headPoseMaps.size() >= kMaxHeadPoseMaps) {
        const QString msg = QStringLiteral("At most %1 maps").arg(kMaxHeadPoseMaps);
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    HeadPoseMap m = AppSettings::makeDefaultHeadPoseMap();
    m.source = m_headChartAxis;
    const QString id = m.id;
    if (m_mutate) {
        m_mutate([m](AppSettings& s) { s.headPoseMaps.push_back(m); }, QStringLiteral("Map added"));
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
    bool found = false;
    for (const HeadPoseMap& m : m_settings.headPoseMaps) {
        if (m.id == mapId) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (error) {
            *error = QStringLiteral("Unknown map");
        }
        return false;
    }
    m_headMapId = mapId;
    m_headPointIndex = 0;
    endCurveScrub();
    return presentLive(m_headMap, QLatin1String(kLiveHeadPoseMap), buildHeadPoseEditor(), error);
}

void SettingsUi::refreshHeadPoseEditor()
{
    if (!m_headMap.active) {
        return;
    }
    QString err;
    (void)m_pages.attachDocument(buildHeadPoseEditor(), &err, false, false);
    syncHeadPosePaint();
}

void SettingsUi::syncHeadPosePaint()
{
    PageHostWindow* w = m_pages.window();
    if (!w) {
        return;
    }
    if (m_headMap.active) {
        if (m_curveScrub) {
            return;
        }
        const HeadPoseMap* m = headPoseDraft();
        if (!m) {
            return;
        }
        const double liveIn = m_headPose ? m_headPose->axisRelative(m->source) : 0.0;
        w->setCurve(m->points, m_headPointIndex, liveIn, m_headPose != nullptr);
        return;
    }
    const HeadPoseMap* chart = mapForChartAxis();
    const QVector<HeadPoseCurvePoint> pts =
        chart ? chart->points : defaultHeadPoseMap().points;
    const double liveIn = m_headPose ? m_headPose->axisRelative(m_headChartAxis) : 0.0;
    w->setCurve(pts, -1, liveIn, m_headPose != nullptr);
}

PageDocument SettingsUi::buildHeadPoseEditor() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveHeadPoseMap);
    doc.name = QStringLiteral("Edit map");
    const ThemeColors theme = m_settings.resolvedTheme();
    initGrid(doc, 12, 6, 1080, 900, 6, 12, theme);
    PageGrid& grid = doc.grids[0];
    adoptCallerBoard(grid, m_pages.pageBehind(QLatin1String(kLiveHeadPoseMap)));
    grid.rowTracks = starTracks({0.85, 1.15, 1.7, 4.2, 1.15, 0.95});
    const EditorSwatch sw = editorSwatch();
    const HeadPoseMap* m = headPoseDraft();
    HeadPoseMap fallback = defaultHeadPoseMap();
    if (!m) {
        m = &fallback;
    }

    PageCell title =
        cell(QStringLiteral("title"), QStringLiteral("Map"), 0, 0, {}, QColor(), 8,
             QStringLiteral("label"),
             QStringLiteral("%1  →  %2")
                 .arg(QLatin1String(headPoseAxisLabel(m->source)), destChipLabel(m->dest)));
    title.textStyle = QStringLiteral("title");
    grid.cells.push_back(title);
    PageCell en =
        cell(QStringLiteral("en"), m->enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled"),
             0, 8, QStringLiteral("headPose.map.enabled.toggle"), QColor(), 2,
             QStringLiteral("toggle"));
    en.activeState = QStringLiteral("headPose.map.enabled");
    grid.cells.push_back(en);
    grid.cells.push_back(cell(QStringLiteral("done"), QStringLiteral("Done"), 0, 10,
                              QStringLiteral("headPose.map.done"), sw.save, 2, {}, {},
                              QStringLiteral("check")));

    grid.cells.push_back(sectionLabel(QStringLiteral("h_from"), QStringLiteral("From"), 1, 0, 2));
    PageGrid from = makeNested(QStringLiteral("from"), 1, 2, 1, 6, 0);
    from.colSpan = 10;
    const HeadPoseAxis axes[] = {HeadPoseAxis::Yaw, HeadPoseAxis::Pitch, HeadPoseAxis::Roll,
                                 HeadPoseAxis::X,   HeadPoseAxis::Y,     HeadPoseAxis::Z};
    for (int i = 0; i < 6; ++i) {
        const QString id = QLatin1String(headPoseAxisId(axes[i]));
        PageCell c = cell(QStringLiteral("src_%1").arg(id),
                          QString::fromLatin1(headPoseAxisLabel(axes[i])), 0, i,
                          QStringLiteral("headPose.map.source.%1").arg(id), QColor(), 1,
                          QStringLiteral("choice"));
        c.activeState = QStringLiteral("headPose.map.source.%1").arg(id);
        joinEnds(c, i, 6);
        from.cells.push_back(c);
    }
    grid.subGrids.push_back(std::move(from));

    grid.cells.push_back(sectionLabel(QStringLiteral("h_to"), QStringLiteral("To"), 2, 0, 2));
    PageGrid to = makeNested(QStringLiteral("to"), 2, 2, 2, 6, 0);
    to.colSpan = 10;
    to.gapPx = 4;
    const HeadPoseDest dests[] = {
        HeadPoseDest::MouseX, HeadPoseDest::MouseY, HeadPoseDest::ScrollV, HeadPoseDest::ScrollH,
        HeadPoseDest::GazeX,  HeadPoseDest::GazeY,  HeadPoseDest::JoyLX,   HeadPoseDest::JoyLY,
        HeadPoseDest::JoyRX,  HeadPoseDest::JoyRY,  HeadPoseDest::Command};
    for (int i = 0; i < 11; ++i) {
        const int row = i < 6 ? 0 : 1;
        const int col = i < 6 ? i : i - 6;
        const QString id = QLatin1String(headPoseDestId(dests[i]));
        PageCell c = cell(QStringLiteral("dst_%1").arg(id), destChipLabel(dests[i]), row, col,
                          QStringLiteral("headPose.map.dest.%1").arg(id), QColor(), 1,
                          QStringLiteral("choice"), {}, destChipIcon(dests[i]));
        c.activeState = QStringLiteral("headPose.map.dest.%1").arg(id);
        if (i == 10) {
            c.colSpan = 2;
            joinEnds(c, 4, 5);
        } else if (i < 6) {
            joinEnds(c, col, 6);
        } else {
            joinEnds(c, col, 5);
        }
        to.cells.push_back(c);
    }
    grid.subGrids.push_back(std::move(to));

    PageCell curve = cell(QStringLiteral("curve"), {}, 3, 0, QStringLiteral("headPose.map.curve.scrub"),
                          QColor(), 12, QStringLiteral("curvefield"), curveCaption(m->points));
    grid.cells.push_back(curve);

    const int nPts = m->points.size();
    const int pi = qBound(0, m_headPointIndex, qMax(0, nPts - 1));
    const HeadPoseCurvePoint pt =
        (pi >= 0 && pi < nPts) ? m->points[pi] : HeadPoseCurvePoint{};

    PageGrid points = makeNested(QStringLiteral("points"), 4, 0, 1, 12, 6);
    points.colSpan = 12;
    PageGrid pick = makeNested(QStringLiteral("pt_pick"), 0, 0, 1, 5, 0);
    pick.colSpan = 4;
    PageCell prev = cell(QStringLiteral("pt_prev"), QStringLiteral("Previous"), 0, 0,
                         QStringLiteral("headPose.map.point.prev"), sw.nudge, 1, {}, {},
                         QStringLiteral("ArrowLeft"));
    joinEnds(prev, 0, 5);
    pick.cells.push_back(prev);
    PageCell lab = cell(QStringLiteral("pt_lab"), QStringLiteral("Point %1 of %2").arg(pi + 1).arg(nPts),
                        0, 1, {}, sw.value, 1, QStringLiteral("value"));
    joinEnds(lab, 1, 5);
    pick.cells.push_back(lab);
    PageCell next = cell(QStringLiteral("pt_next"), QStringLiteral("Next"), 0, 2,
                         QStringLiteral("headPose.map.point.next"), sw.nudge, 1, {}, {},
                         QStringLiteral("ArrowRight"));
    joinEnds(next, 2, 5);
    pick.cells.push_back(next);
    PageCell add = cell(QStringLiteral("pt_add"), QStringLiteral("Add"), 0, 3,
                        QStringLiteral("headPose.map.point.add"), sw.add, 1, {}, {},
                        QStringLiteral("add"));
    joinEnds(add, 3, 5);
    pick.cells.push_back(add);
    PageCell delPt = cell(QStringLiteral("pt_del"), QStringLiteral("Remove"), 0, 4,
                          QStringLiteral("headPose.map.point.del"), sw.cancel, 1, {}, {},
                          QStringLiteral("delete"));
    joinEnds(delPt, 4, 5);
    pick.cells.push_back(delPt);
    points.subGrids.push_back(std::move(pick));

    const QString inUnit = sourceUnit(m->source);
    PageGrid in = makeNested(QStringLiteral("in_step"), 0, 4, 1, 4, 0);
    in.colSpan = 4;
    PageCell inDec = cell(QStringLiteral("in_dec"), QStringLiteral("−"), 0, 0,
                          QStringLiteral("headPose.map.point.in.dec"), sw.nudge);
    joinEnds(inDec, 0, 4);
    in.cells.push_back(inDec);
    PageCell inVal = cell(QStringLiteral("in_val"), formatAxisValue(pt.in, inUnit), 0, 1, {}, sw.value,
                          1, QStringLiteral("value"), QStringLiteral("Input"));
    joinEnds(inVal, 1, 4);
    in.cells.push_back(inVal);
    PageCell inInc = cell(QStringLiteral("in_inc"), QStringLiteral("+"), 0, 2,
                          QStringLiteral("headPose.map.point.in.inc"), sw.nudge);
    joinEnds(inInc, 2, 4);
    in.cells.push_back(inInc);
    PageCell inEdit = cell(QStringLiteral("in_edit"), QStringLiteral("Edit"), 0, 3,
                           QStringLiteral("headPose.map.point.in.edit"), sw.edit, 1, {}, {},
                           QStringLiteral("editSquare"));
    joinEnds(inEdit, 3, 4);
    in.cells.push_back(inEdit);
    points.subGrids.push_back(std::move(in));

    const QString outU = destUnit(m->dest);
    PageGrid out = makeNested(QStringLiteral("out_step"), 0, 8, 1, 4, 0);
    out.colSpan = 4;
    PageCell outDec = cell(QStringLiteral("out_dec"), QStringLiteral("−"), 0, 0,
                           QStringLiteral("headPose.map.point.out.dec"), sw.nudge);
    joinEnds(outDec, 0, 4);
    out.cells.push_back(outDec);
    PageCell outVal =
        cell(QStringLiteral("out_val"), formatAxisValue(pt.out, outU), 0, 1, {}, sw.value, 1,
             QStringLiteral("value"),
             m->dest == HeadPoseDest::Command ? QStringLiteral("Unused") : QStringLiteral("Output"));
    joinEnds(outVal, 1, 4);
    out.cells.push_back(outVal);
    PageCell outInc = cell(QStringLiteral("out_inc"), QStringLiteral("+"), 0, 2,
                           QStringLiteral("headPose.map.point.out.inc"), sw.nudge);
    joinEnds(outInc, 2, 4);
    out.cells.push_back(outInc);
    PageCell outEdit = cell(QStringLiteral("out_edit"), QStringLiteral("Edit"), 0, 3,
                            QStringLiteral("headPose.map.point.out.edit"), sw.edit, 1, {}, {},
                            QStringLiteral("editSquare"));
    joinEnds(outEdit, 3, 4);
    out.cells.push_back(outEdit);
    points.subGrids.push_back(std::move(out));
    grid.subGrids.push_back(std::move(points));

    grid.cells.push_back(cell(QStringLiteral("del"), QStringLiteral("Delete map"), 5, 0,
                              QStringLiteral("headPose.map.delete"), sw.cancel, 3, {}, {},
                              QStringLiteral("delete")));
    if (m->dest == HeadPoseDest::Command) {
        grid.cells.push_back(cell(QStringLiteral("cmd"),
                                  m->command.isEmpty() ? QStringLiteral("Choose command") : m->command,
                                  5, 3, QStringLiteral("headPose.map.pickCommand"), sw.edit, 5, {},
                                  {}, QStringLiteral("adsClick")));
        PageGrid trig = makeNested(QStringLiteral("trig"), 5, 8, 1, 4, 0);
        trig.colSpan = 4;
        PageCell atDec = cell(QStringLiteral("at_dec"), QStringLiteral("−"), 0, 0,
                              QStringLiteral("headPose.map.commandAt.dec"), sw.nudge);
        joinEnds(atDec, 0, 4);
        trig.cells.push_back(atDec);
        PageCell atVal =
            cell(QStringLiteral("at_val"), formatAxisValue(m->commandAt, sourceUnit(m->source)), 0, 1,
                 {}, sw.value, 2, QStringLiteral("value"), QStringLiteral("Trigger"));
        joinEnds(atVal, 1, 4);
        trig.cells.push_back(atVal);
        PageCell atInc = cell(QStringLiteral("at_inc"), QStringLiteral("+"), 0, 3,
                              QStringLiteral("headPose.map.commandAt.inc"), sw.nudge);
        joinEnds(atInc, 3, 4);
        trig.cells.push_back(atInc);
        grid.subGrids.push_back(std::move(trig));
    }
    return doc;
}

QStringList SettingsUi::headPoseCommandCatalog() const
{
    QStringList names;
    for (const QString& n : m_commands.names()) {
        if (n.startsWith(QLatin1String("settings.")) || n.startsWith(QLatin1String("headPose."))
            || n.startsWith(QLatin1String("theme.")) || n.startsWith(QLatin1String("compose."))
            || n.startsWith(QLatin1String("speech.")) || n.startsWith(QLatin1String("soundboard."))
            || n.startsWith(QLatin1String("history.")) || n.startsWith(QLatin1String("gazer."))
            || n.startsWith(QLatin1String("lts."))) {
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
    m_headCmdPage = 0;
    return presentLive(m_headCmd, QLatin1String(kLiveHeadPoseCmd), buildHeadPoseCommandList(),
                       error);
}

void SettingsUi::refreshHeadPoseCommandList()
{
    if (!m_headCmd.active) {
        return;
    }
    QString err;
    (void)m_pages.attachDocument(buildHeadPoseCommandList(), &err, false, false);
}

PageDocument SettingsUi::buildHeadPoseCommandList() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveHeadPoseCmd);
    doc.name = QStringLiteral("Command");
    const ThemeColors theme = m_settings.resolvedTheme();
    initGrid(doc, 4, 6, 860, 640, 8, 16, theme);
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = starTracks({0.9, 1.2, 1.2, 1.2, 1.2, 0.95});
    const EditorSwatch sw = editorSwatch();
    const QStringList names = headPoseCommandCatalog();
    constexpr int kPage = 12;
    const int pages = qMax(1, (names.size() + kPage - 1) / kPage);
    const int page = qBound(0, m_headCmdPage, pages - 1);
    const int start = page * kPage;
    PageCell title = cell(QStringLiteral("title"), QStringLiteral("Choose command"), 0, 0, {},
                          QColor(), 3, QStringLiteral("label"),
                          QStringLiteral("Page %1 of %2").arg(page + 1).arg(pages));
    title.textStyle = QStringLiteral("title");
    grid.cells.push_back(title);
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 0, 3,
                              QStringLiteral("headPose.cmdList.cancel"), sw.cancel));
    for (int i = 0; i < kPage; ++i) {
        const int idx = start + i;
        const int row = 1 + i / 4;
        const int col = i % 4;
        if (idx >= names.size()) {
            grid.cells.push_back(cell(QStringLiteral("c_%1").arg(i), {}, row, col, {}, QColor(), 1,
                                      QStringLiteral("label")));
            continue;
        }
        PageCell c = cell(QStringLiteral("c_%1").arg(i), names[idx], row, col,
                          QStringLiteral("headPose.cmd.%1").arg(encodeCmd(names[idx])), sw.nudge);
        grid.cells.push_back(c);
    }
    PageCell prev = cell(QStringLiteral("prev"), QStringLiteral("Previous"), 5, 0,
                         QStringLiteral("headPose.cmdList.prev"), sw.nudge, 1, {}, {},
                         QStringLiteral("ArrowLeft"));
    joinEnds(prev, 0, 3);
    grid.cells.push_back(prev);
    PageCell pg = cell(QStringLiteral("pg"), QStringLiteral("%1 / %2").arg(page + 1).arg(pages), 5, 1,
                       {}, sw.value, 1, QStringLiteral("value"));
    joinEnds(pg, 1, 3);
    grid.cells.push_back(pg);
    PageCell next = cell(QStringLiteral("next"), QStringLiteral("Next"), 5, 2,
                         QStringLiteral("headPose.cmdList.next"), sw.nudge, 2, {}, {},
                         QStringLiteral("ArrowRight"));
    joinEnds(next, 2, 3);
    grid.cells.push_back(next);
    return doc;
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
    if (!m_headMap.active) {
        endCurveScrub();
        return;
    }
    const qint64 now = point.timestampMs;
    const QRect r =
        m_pages.targetScreenRect(QLatin1String(kLiveHeadPoseMap), QStringLiteral("curve"));
    const bool onCurve =
        point.valid && !r.isEmpty() && r.contains(point.toPointF().toPoint());
    if (!onCurve) {
        if (m_curveLeaveGrace.onInvalid(now) == InvalidGazeGrace::Result::Holding) {
            return;
        }
        endCurveScrub();
        return;
    }
    m_curveLeaveGrace.onValid();
    HeadPoseMap* m = headPoseDraft();
    if (!m || m->points.isEmpty()) {
        endCurveScrub();
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
    }
}

} // namespace gazer

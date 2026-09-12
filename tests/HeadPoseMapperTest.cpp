#include "mapping/HeadPoseCurve.h"
#include "ui/PreviewGeometry.h"

#include <QtTest>

using namespace gazer;

class HeadPoseMapperTest final : public QObject {
    Q_OBJECT

private slots:
    void curveLerp();
    void curveClamps();
    void originSubtract();
    void recenterCapturesAllSixAxes();
    void previewRelativeZSkipsRestCm();
    void mouseVelocityIntegrates();
    void pauseZerosMotion();
    void commandRisingEdge();
    void commandHysteresis();
};

void HeadPoseMapperTest::curveLerp()
{
    const QVector<HeadPoseCurvePoint> pts = {{-10.0, -100.0}, {0.0, 0.0}, {10.0, 100.0}};
    QCOMPARE(evalHeadPoseCurve(pts, 0.0), 0.0);
    QCOMPARE(evalHeadPoseCurve(pts, 5.0), 50.0);
    QCOMPARE(evalHeadPoseCurve(pts, -5.0), -50.0);
    QCOMPARE(evalHeadPoseCurve(pts, 20.0), 100.0);
    QCOMPARE(evalHeadPoseCurve(pts, -20.0), -100.0);
}

void HeadPoseMapperTest::curveClamps()
{
    HeadPoseMap m = defaultHeadPoseMap();
    m.points = {{5.0, 1.0}, {5.0, 2.0}, {-5.0, -1.0}};
    clampHeadPoseMap(m);
    QCOMPARE(m.points.size(), 2);
    QCOMPARE(m.points.first().in, -5.0);
    QCOMPARE(m.points.last().in, 5.0);
    QCOMPARE(m.points.last().out, 2.0);
}

void HeadPoseMapperTest::originSubtract()
{
    HeadPoseMap m = defaultHeadPoseMap();
    m.id = QStringLiteral("a");
    m.points = {{-10.0, -10.0}, {0.0, 0.0}, {10.0, 10.0}};
    m.dest = HeadPoseDest::GazeX;
    HeadPose pose;
    pose.rotationValid = true;
    pose.yaw = 20.0;
    HeadPose origin;
    origin.rotationValid = true;
    origin.yaw = 10.0;
    HeadPoseEvalState st;
    const HeadPoseOutputs out =
        evalHeadPoseMaps({m}, true, pose, origin, true, false, 100, &st);
    QCOMPARE(out.gazeOffset.x(), 10.0);
}

void HeadPoseMapperTest::recenterCapturesAllSixAxes()
{
    HeadPose pose;
    pose.rotationValid = true;
    pose.positionValid = false;
    pose.yaw = 12.0;
    pose.pitch = -4.0;
    pose.roll = 3.0;
    pose.x = 1.5;
    pose.y = -2.0;
    pose.z = 40.0;
    HeadPose origin;
    captureHeadPoseOrigin(origin, pose);
    QVERIFY(origin.rotationValid);
    QVERIFY(origin.positionValid);
    QCOMPARE(origin.yaw, 12.0);
    QCOMPARE(origin.pitch, -4.0);
    QCOMPARE(origin.roll, 3.0);
    QCOMPARE(origin.x, 1.5);
    QCOMPARE(origin.y, -2.0);
    QCOMPARE(origin.z, 40.0);

    HeadPose now = pose;
    now.positionValid = true;
    now.yaw = 12.0;
    now.x = 1.5;
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::Yaw), 0.0);
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::Pitch), 0.0);
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::Roll), 0.0);
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::X), 0.0);
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::Y), 0.0);
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::Z), 0.0);

    now.yaw = 22.0;
    now.x = 3.5;
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::Yaw), 10.0);
    QCOMPARE(headPoseAxisRelative(now, origin, true, HeadPoseAxis::X), 2.0);

    HeadPoseMap mx = defaultHeadPoseMap();
    mx.id = QStringLiteral("x");
    mx.source = HeadPoseAxis::X;
    mx.dest = HeadPoseDest::GazeX;
    mx.points = {{-10.0, -10.0}, {0.0, 0.0}, {10.0, 10.0}};
    HeadPoseEvalState st;
    const HeadPoseOutputs out = evalHeadPoseMaps({mx}, true, now, origin, true, false, 16, &st);
    QCOMPARE(out.gazeOffset.x(), 2.0);
}

void HeadPoseMapperTest::previewRelativeZSkipsRestCm()
{
    using gazer::PreviewGeom::headMeshTz;
    using gazer::PreviewGeom::kHeadRestZCm;
    QVERIFY(qFuzzyIsNull(headMeshTz(0.0, true)));
    QVERIFY(qFuzzyIsNull(headMeshTz(kHeadRestZCm, false)));
    QVERIFY(headMeshTz(0.0, false) < -1.0);
}

void HeadPoseMapperTest::mouseVelocityIntegrates()
{
    HeadPoseMap m = defaultHeadPoseMap();
    m.id = QStringLiteral("m");
    m.dest = HeadPoseDest::MouseX;
    m.points = {{-1.0, -100.0}, {0.0, 0.0}, {1.0, 100.0}};
    HeadPose pose;
    pose.rotationValid = true;
    pose.yaw = 1.0;
    HeadPoseEvalState st;
    const HeadPoseOutputs out =
        evalHeadPoseMaps({m}, true, pose, {}, false, false, 100, &st);
    QCOMPARE(out.mouseDx, 10);
}

void HeadPoseMapperTest::pauseZerosMotion()
{
    HeadPoseMap m = defaultHeadPoseMap();
    m.id = QStringLiteral("m");
    m.dest = HeadPoseDest::MouseX;
    HeadPose pose;
    pose.rotationValid = true;
    pose.yaw = 25.0;
    HeadPoseEvalState st;
    const HeadPoseOutputs out =
        evalHeadPoseMaps({m}, true, pose, {}, false, true, 50, &st);
    QCOMPARE(out.mouseDx, 0);
    QCOMPARE(out.gazeOffset, QPointF());
}

void HeadPoseMapperTest::commandRisingEdge()
{
    HeadPoseMap m;
    m.id = QStringLiteral("c");
    m.dest = HeadPoseDest::Command;
    m.command = QStringLiteral("mouseLeftClick");
    m.commandAt = 10.0;
    m.hysteresis = 2.0;
    m.points = {{0.0, 0.0}, {1.0, 1.0}};
    HeadPose pose;
    pose.rotationValid = true;
    pose.yaw = 5.0;
    HeadPoseEvalState st;
    auto a = evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    QVERIFY(a.commands.isEmpty());
    pose.yaw = 11.0;
    auto b = evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    QCOMPARE(b.commands.size(), 1);
    QCOMPARE(b.commands.first(), QStringLiteral("mouseLeftClick"));
    auto c = evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    QVERIFY(c.commands.isEmpty());
}

void HeadPoseMapperTest::commandHysteresis()
{
    HeadPoseMap m;
    m.id = QStringLiteral("c");
    m.dest = HeadPoseDest::Command;
    m.command = QStringLiteral("x");
    m.commandAt = 10.0;
    m.hysteresis = 3.0;
    m.points = {{0.0, 0.0}, {1.0, 1.0}};
    HeadPose pose;
    pose.rotationValid = true;
    pose.yaw = 12.0;
    HeadPoseEvalState st;
    (void)evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    pose.yaw = 8.5;
    auto mid = evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    QVERIFY(mid.commands.isEmpty());
    pose.yaw = 12.0;
    auto still = evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    QVERIFY(still.commands.isEmpty());
    pose.yaw = 6.0;
    (void)evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    pose.yaw = 12.0;
    auto again = evalHeadPoseMaps({m}, true, pose, {}, false, false, 16, &st);
    QCOMPARE(again.commands.size(), 1);
}

QObject* createHeadPoseMapperTest()
{
    return new HeadPoseMapperTest;
}

#include "HeadPoseMapperTest.moc"

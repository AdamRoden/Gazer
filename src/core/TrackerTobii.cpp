#include "core/TrackerTobii.h"

#include "core/TrackerMouse.h"
#include "utils/Heartbeat.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <cmath>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QScopeGuard>
#include <QScreen>

#include <chrono>

namespace gazer {

TrackerTobii::TrackerTobii(QObject* parent)
    : ITracker(parent)
{
    m_flushTimer.setInterval(16);
    m_flushTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_flushTimer, &QTimer::timeout, this, &TrackerTobii::flushPendingGaze);

    if (auto* app = qGuiApp) {
        connect(app, &QGuiApplication::primaryScreenChanged, this, [this](QScreen*) {
            bindOverlayScreen();
        });
        connect(app, &QGuiApplication::screenAdded, this, [this](QScreen*) {
            bindOverlayScreen();
        });
        connect(app, &QGuiApplication::screenRemoved, this, [this](QScreen*) {
            bindOverlayScreen();
        });
    }
    bindOverlayScreen();
}

TrackerTobii::~TrackerTobii()
{
    stop();
}

bool TrackerTobii::workerOutstanding()
{
    std::lock_guard lock(m_exitMutex);
    return !m_workerExited;
}

void TrackerTobii::retire()
{
    disconnect(this, nullptr, nullptr, nullptr);
    bool deleteNow = false;
    {
        std::lock_guard lock(m_exitMutex);
        m_retire = true;
        deleteNow = m_workerExited;
    }
    if (deleteNow) {
        delete this;
    }
}

std::unique_ptr<ITracker> openEyeTracker()
{
    {
        auto tobii = std::make_unique<TrackerTobii>();
        if (tobii->start()) {
            return tobii;
        }
        GAZER_INFO << "Tobii unavailable — falling back to mouse cursor";
        releaseTracker(tobii);
    }
    auto mouse = std::make_unique<TrackerMouse>();
    if (!mouse->start()) {
        GAZER_ERROR << "Mouse tracker failed to start";
        return {};
    }
    return mouse;
}

QString TrackerTobii::name() const
{
    return QStringLiteral("Tobii");
}

bool TrackerTobii::isRunning() const
{
    return m_running.load();
}

void TrackerTobii::urlReceiver(char const* url, void* user_data)
{
    auto* urls = static_cast<std::vector<std::string>*>(user_data);
    if (url && urls) {
        urls->emplace_back(url);
    }
}

void TrackerTobii::gazePointCallback(tobii_gaze_point_t const* gaze_point, void* user_data)
{
    auto* self = static_cast<TrackerTobii*>(user_data);
    if (self && gaze_point) {
        self->onGazeFromEngine(gaze_point);
    }
}

void TrackerTobii::headPoseCallback(tobii_head_pose_t const* head_pose, void* user_data)
{
    auto* self = static_cast<TrackerTobii*>(user_data);
    if (self && head_pose) {
        self->onHeadFromEngine(head_pose);
    }
}

void TrackerTobii::bindOverlayScreen()
{
    if (m_overlayScreen) {
        disconnect(m_overlayScreen, nullptr, this, nullptr);
    }
    m_overlayScreen = QGuiApplication::primaryScreen();
    if (m_overlayScreen) {
        connect(m_overlayScreen, &QScreen::geometryChanged, this, [this](const QRect&) {
            cacheScreenGeometry();
        });
        connect(m_overlayScreen, &QScreen::virtualGeometryChanged, this, [this](const QRect&) {
            cacheScreenGeometry();
        });
    }
    cacheScreenGeometry();
}

void TrackerTobii::cacheScreenGeometry()
{
    const QRect geo = overlayScreenGeometry();
    QMutexLocker lock(&m_geoMutex);
    m_screenGeo = geo;
}

void TrackerTobii::onGazeFromEngine(tobii_gaze_point_t const* gp)
{
    // Worker thread only: no Qt signals from here.
    GazePoint out;
    out.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    out.valid = (gp->validity == TOBII_VALIDITY_VALID);

    if (out.valid) {
        QRect geo;
        {
            QMutexLocker lock(&m_geoMutex);
            geo = m_screenGeo;
        }
        if (geo.isValid()) {
            // Map display-normalized coords to Qt screen space WITHOUT clamping.
            // Tobii may report position_xy outside [0,1] when gaze is past the
            // bezel; keep those samples so off-screen dwell regions are hittable.
            const double nx = static_cast<double>(gp->position_xy[0]);
            const double ny = static_cast<double>(gp->position_xy[1]);
            out.x = geo.x() + nx * static_cast<double>(geo.width());
            out.y = geo.y() + ny * static_cast<double>(geo.height());
        } else {
            out.valid = false;
        }
    }

    {
        QMutexLocker lock(&m_sampleMutex);
        m_latestSample = out;
    }
    m_samplePending = true;
    Heartbeat::publishGaze(int(std::lround(out.x)), int(std::lround(out.y)), out.valid);
}

void TrackerTobii::onHeadFromEngine(tobii_head_pose_t const* hp)
{
    // Worker thread: convert to OpenTrack-friendly degrees / cm.
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    HeadPose out;
    out.timestampMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    out.positionValid = (hp->position_validity == TOBII_VALIDITY_VALID);
    // rotation_xyz is pitch, yaw, roll — same order as the existing copy below.
    out.pitchValid = (hp->rotation_validity_xyz[0] == TOBII_VALIDITY_VALID);
    out.yawValid = (hp->rotation_validity_xyz[1] == TOBII_VALIDITY_VALID);
    out.rollValid = (hp->rotation_validity_xyz[2] == TOBII_VALIDITY_VALID);
    out.rotationValid = out.pitchValid || out.yawValid || out.rollValid;
    if (out.positionValid) {
        out.x = -static_cast<double>(hp->position_xyz[0]) * 0.1; // mm → cm, negate X
        out.y = static_cast<double>(hp->position_xyz[1]) * 0.1;
        out.z = static_cast<double>(hp->position_xyz[2]) * 0.1;
    }
    if (out.pitchValid) {
        out.pitch = static_cast<double>(hp->rotation_xyz[0]) * kRadToDeg;
    }
    if (out.yawValid) {
        out.yaw = -static_cast<double>(hp->rotation_xyz[1]) * kRadToDeg;
    }
    if (out.rollValid) {
        out.roll = static_cast<double>(hp->rotation_xyz[2]) * kRadToDeg;
    }
    {
        QMutexLocker lock(&m_sampleMutex);
        m_latestHead = out;
    }
    m_headPending = true;
}

void TrackerTobii::flushPendingGaze()
{
    if (!m_running.load() && !m_samplePending.load() && !m_headPending.load()) {
        return;
    }

    if (m_samplePending.exchange(false)) {
        GazePoint sample;
        {
            QMutexLocker lock(&m_sampleMutex);
            sample = m_latestSample;
        }

        // Validity transitions on GUI thread only (no queued worker lambdas).
        if (sample.valid && !m_guiHadValid) {
            m_guiHadValid = true;
            emit trackingRestored();
        } else if (!sample.valid && m_guiHadValid) {
            m_guiHadValid = false;
            emit trackingLost();
        }

        emit gazeUpdated(sample);
    }

    if (m_headPending.exchange(false)) {
        HeadPose head;
        {
            QMutexLocker lock(&m_sampleMutex);
            head = m_latestHead;
        }
        emit headPoseUpdated(head);
    }
}

bool TrackerTobii::start()
{
    if (m_running.load() || m_thread) {
        return true;
    }

#if !GAZER_USE_TOBII
    GAZER_INFO << "TrackerTobii: built with GAZER_USE_TOBII=0";
    return false;
#endif

    // Refuse restart until any previous worker has fully exited (no detach+restart race).
    {
        std::unique_lock lock(m_exitMutex);
        if (!m_workerExited) {
            if (!m_exitCv.wait_for(lock, std::chrono::seconds(5),
                                   [this]() { return m_workerExited; })) {
                GAZER_WARN << "TrackerTobii: previous worker still running — cannot start";
                return false;
            }
        }
    }
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
        m_thread.reset();
    }

    QString err;
    if (!m_lib.load(&err)) {
        GAZER_WARN << "TrackerTobii: cannot load Stream Engine —" << err;
        return false;
    }

    tobii_version_t ver{};
    if (m_lib.get_api_version(&ver) == TOBII_ERROR_NO_ERROR) {
        GAZER_INFO << "Stream Engine" << ver.major << "." << ver.minor << "." << ver.revision;
    }

    cacheScreenGeometry();
    m_clock.start();
    m_guiHadValid = false;
    m_samplePending = false;
    m_stop = false;
    m_setupDone = false;
    m_setupOk = false;
    m_setupError.clear();
    {
        std::lock_guard lock(m_exitMutex);
        m_workerExited = false;
    }
    ++m_generation;

    m_thread = std::make_unique<std::thread>(&TrackerTobii::workerMain, this);
    // Device discovery stays on the worker. Callers keep running; streamFailed
    // reports a setup failure once this returns true.
    return true;
}

void TrackerTobii::workerMain()
{
    // Always mark the worker finished, including setup-failure returns.
    // A retired tracker is deleted here; otherwise a failed setup emits streamFailed.
    const auto exited = qScopeGuard([this] {
        const bool setupFailed = m_setupDone && !m_setupOk;
        const QString reason = m_setupError;
        const uint64_t gen = m_generation.load();
        bool retire = false;
        {
            std::lock_guard lock(m_exitMutex);
            m_workerExited = true;
            retire = m_retire;
            m_exitCv.notify_all();
        }
        if (retire) {
            TrackerTobii* self = this;
            if (QCoreApplication* app = QCoreApplication::instance()) {
                QMetaObject::invokeMethod(app, [self]() { delete self; }, Qt::QueuedConnection);
            } else {
                delete self;
            }
            return;
        }
        if (!setupFailed || m_stop.load()) {
            return;
        }
        const QString why = reason.isEmpty() ? QStringLiteral("Tobii setup failed") : reason;
        QMetaObject::invokeMethod(
            this,
            [this, why, gen]() {
                if (m_generation.load() != gen || m_stop.load()) {
                    return;
                }
                emit streamFailed(why);
            },
            Qt::QueuedConnection);
    });

    // Entire device lifecycle stays on this thread.
    tobii_error_t e = m_lib.api_create(&m_api, nullptr, nullptr);
    if (e != TOBII_ERROR_NO_ERROR || !m_api) {
        std::lock_guard lock(m_setupMutex);
        m_setupError = QStringLiteral("api_create: %1").arg(m_lib.errorString(e));
        m_setupOk = false;
        m_setupDone = true;
        m_setupCv.notify_all();
        return;
    }

    m_urls.clear();
    e = m_lib.enumerate_local_device_urls(m_api, &TrackerTobii::urlReceiver, &m_urls);
    if (e != TOBII_ERROR_NO_ERROR || m_urls.empty()) {
        std::lock_guard lock(m_setupMutex);
        m_setupError = m_urls.empty() ? QStringLiteral("no devices found")
                                      : QStringLiteral("enumerate: %1").arg(m_lib.errorString(e));
        m_setupOk = false;
        m_setupDone = true;
        m_setupCv.notify_all();
        teardownDeviceUnlocked();
        return;
    }

    e = m_lib.device_create(m_api, m_urls.front().c_str(), TOBII_FIELD_OF_USE_INTERACTIVE,
                            &m_device);
    if (e != TOBII_ERROR_NO_ERROR || !m_device) {
        std::lock_guard lock(m_setupMutex);
        m_setupError = QStringLiteral("device_create: %1").arg(m_lib.errorString(e));
        m_setupOk = false;
        m_setupDone = true;
        m_setupCv.notify_all();
        teardownDeviceUnlocked();
        return;
    }

    e = m_lib.gaze_point_subscribe(m_device, &TrackerTobii::gazePointCallback, this);
    if (e != TOBII_ERROR_NO_ERROR) {
        std::lock_guard lock(m_setupMutex);
        m_setupError = QStringLiteral("subscribe: %1").arg(m_lib.errorString(e));
        m_setupOk = false;
        m_setupDone = true;
        m_setupCv.notify_all();
        teardownDeviceUnlocked();
        return;
    }

    m_headPoseSubscribed = false;
    if (m_lib.head_pose_subscribe) {
        e = m_lib.head_pose_subscribe(m_device, &TrackerTobii::headPoseCallback, this);
        if (e == TOBII_ERROR_NO_ERROR) {
            m_headPoseSubscribed = true;
            GAZER_INFO << "Tobii head pose stream subscribed";
        } else {
            GAZER_WARN << "Tobii head pose subscribe skipped:" << m_lib.errorString(e);
        }
    }

    {
        std::lock_guard lock(m_setupMutex);
        m_setupOk = true;
        m_setupDone = true;
        m_setupCv.notify_all();
    }

    {
        const uint64_t gen = m_generation.load();
        QMetaObject::invokeMethod(
            this,
            [this, gen]() {
                if (m_generation.load() != gen || m_stop.load()) {
                    return;
                }
                m_running = true;
                m_flushTimer.start();
                GAZER_INFO << "TrackerTobii started";
            },
            Qt::QueuedConnection);
    }

    GAZER_INFO << "Tobii device:" << QString::fromStdString(m_urls.front());

    QString failReason;
    while (!m_stop.load()) {
        tobii_device_t* devices[1] = {m_device};
        e = m_lib.wait_for_callbacks(1, devices);
        if (m_stop.load()) {
            break;
        }

        if (e == TOBII_ERROR_TIMED_OUT) {
            continue;
        }
        if (e == TOBII_ERROR_CONNECTION_FAILED) {
            GAZER_WARN << "wait_for_callbacks: reconnecting";
            bool reconnected = false;
            for (int i = 0; i < 30 && !m_stop.load(); ++i) {
                e = m_lib.device_reconnect(m_device);
                if (e == TOBII_ERROR_NO_ERROR) {
                    reconnected = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!reconnected) {
                failReason = QStringLiteral("reconnect failed: %1").arg(m_lib.errorString(e));
                break;
            }
            continue;
        }
        if (e != TOBII_ERROR_NO_ERROR) {
            failReason = QStringLiteral("wait_for_callbacks: %1").arg(m_lib.errorString(e));
            break;
        }

        e = m_lib.device_process_callbacks(m_device);
        if (e == TOBII_ERROR_CONNECTION_FAILED) {
            GAZER_WARN << "process_callbacks: reconnecting";
            bool reconnected = false;
            for (int i = 0; i < 30 && !m_stop.load(); ++i) {
                e = m_lib.device_reconnect(m_device);
                if (e == TOBII_ERROR_NO_ERROR) {
                    reconnected = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!reconnected) {
                failReason = QStringLiteral("reconnect failed: %1").arg(m_lib.errorString(e));
                break;
            }
            continue;
        }
        if (e != TOBII_ERROR_NO_ERROR) {
            failReason = QStringLiteral("process_callbacks: %1").arg(m_lib.errorString(e));
            break;
        }
    }

    teardownDeviceUnlocked();

    const uint64_t gen = m_generation.load();
    const QString reason = failReason;
    QMetaObject::invokeMethod(
        this,
        [this, gen, reason]() {
            if (m_generation.load() != gen) {
                return;
            }
            onWorkerFinished();
            if (!reason.isEmpty() && !m_stop.load()) {
                emit streamFailed(reason);
            }
        },
        Qt::QueuedConnection);
}

void TrackerTobii::teardownDeviceUnlocked()
{
    if (m_device && m_lib.isLoaded()) {
        if (m_headPoseSubscribed && m_lib.head_pose_unsubscribe) {
            m_lib.head_pose_unsubscribe(m_device);
            m_headPoseSubscribed = false;
        }
        m_lib.gaze_point_unsubscribe(m_device);
        m_lib.device_destroy(m_device);
        m_device = nullptr;
    }
    if (m_api && m_lib.isLoaded()) {
        m_lib.api_destroy(m_api);
        m_api = nullptr;
    }
}

void TrackerTobii::onWorkerFinished()
{
    m_flushTimer.stop();
    m_running = false;
    m_samplePending = false;
    if (m_guiHadValid) {
        m_guiHadValid = false;
        emit trackingLost();
    }
}

void TrackerTobii::stop()
{
    if (!m_thread) {
        m_flushTimer.stop();
        m_running = false;
        return;
    }

    ++m_generation;
    m_stop = true;
    m_flushTimer.stop();

    // Wait for worker teardown (device destroy runs only on the worker).
    // Do NOT null m_api/m_device here — the worker owns them until teardownDeviceUnlocked.
    {
        std::unique_lock lock(m_exitMutex);
        if (!m_exitCv.wait_for(lock, std::chrono::seconds(3),
                               [this]() { return m_workerExited; })) {
            GAZER_WARN << "TrackerTobii: worker did not exit within 3s";
        }
    }

    if (m_thread->joinable()) {
        if (m_workerExited) {
            m_thread->join();
        } else {
            // Detach only as last resort; start() refuses until m_workerExited.
            // Pointers stay owned by the detached worker until it finishes teardown.
            GAZER_WARN << "TrackerTobii: detaching worker; restart blocked until it exits";
            m_thread->detach();
        }
    }
    m_thread.reset();

    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);

    m_running = false;
    m_samplePending = false;
    // m_api / m_device are already null if worker completed teardown; if still
    // non-null, a detached worker is finishing — leave them alone.

    if (m_guiHadValid) {
        m_guiHadValid = false;
        emit trackingLost();
    }
}

} // namespace gazer

#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"
#include "core/ITracker.h"
#include "core/StreamEngineLib.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QPointer>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct tobii_api_t;
struct tobii_device_t;
struct tobii_gaze_point_t;
struct tobii_head_pose_t;

namespace gazer {

/// Tobii Stream Engine backend.
/// Device create/process/destroy all run on the worker thread; GUI only coalesces samples.
class TrackerTobii final : public ITracker {
    Q_OBJECT

public:
    explicit TrackerTobii(QObject* parent = nullptr);
    ~TrackerTobii() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString name() const override;
    /// True while a worker thread may still touch this object.
    [[nodiscard]] bool workerOutstanding();
    /// Caller has dropped ownership. The worker deletes this when it exits;
    /// if the worker is already gone, this deletes itself now.
    void retire();

signals:
    void streamFailed(const QString& reason);

private:
    void flushPendingGaze();
    void onWorkerFinished();

    static void gazePointCallback(tobii_gaze_point_t const* gaze_point, void* user_data);
    static void headPoseCallback(tobii_head_pose_t const* head_pose, void* user_data);
    static void urlReceiver(char const* url, void* user_data);

    void workerMain();
    void onGazeFromEngine(tobii_gaze_point_t const* gaze_point);
    void onHeadFromEngine(tobii_head_pose_t const* head_pose);
    void cacheScreenGeometry();
    void bindOverlayScreen();
    void teardownDeviceUnlocked();

    StreamEngineLib m_lib;
    tobii_api_t* m_api = nullptr;
    tobii_device_t* m_device = nullptr;
    std::vector<std::string> m_urls;
    bool m_headPoseSubscribed = false;

    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_generation{0};

    std::mutex m_setupMutex;
    std::condition_variable m_setupCv;
    bool m_setupDone = false;
    bool m_setupOk = false;
    QString m_setupError;

    std::mutex m_exitMutex;
    std::condition_variable m_exitCv;
    bool m_workerExited = true;
    bool m_retire = false;

    QElapsedTimer m_clock;

    QMutex m_sampleMutex;
    GazePoint m_latestSample;
    HeadPose m_latestHead;
    std::atomic<bool> m_samplePending{false};
    std::atomic<bool> m_headPending{false};
    QTimer m_flushTimer;

    bool m_guiHadValid = false;

    QMutex m_geoMutex;
    QRect m_screenGeo;
    QPointer<QScreen> m_overlayScreen;
};

/// stop(), then delete. A Tobii worker that is still running keeps the object
/// alive until it exits so the destructor does not race the thread.
template <typename T>
void releaseTracker(std::unique_ptr<T>& tracker)
{
    if (!tracker) {
        return;
    }
    tracker->stop();
    if (auto* tobii = dynamic_cast<TrackerTobii*>(tracker.get())) {
        if (tobii->workerOutstanding()) {
            TrackerTobii* raw = tobii;
            tracker.release();
            raw->retire();
            return;
        }
    }
    tracker.reset();
}

/// Tobii when start() accepts it, otherwise the mouse tracker.
/// Device discovery finishes on the Tobii worker; setup failure is streamFailed.
std::unique_ptr<ITracker> openEyeTracker();

} // namespace gazer

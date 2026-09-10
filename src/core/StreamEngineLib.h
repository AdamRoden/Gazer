#pragma once

#include <tobii.h>
#include <tobii_streams.h>

#include <QString>

namespace gazer {

/// Runtime loader for tobii_stream_engine.dll (no static .lib required).
class StreamEngineLib {
public:
    StreamEngineLib();
    ~StreamEngineLib();

    StreamEngineLib(const StreamEngineLib&) = delete;
    StreamEngineLib& operator=(const StreamEngineLib&) = delete;

    /// Try known install paths + app dir. Returns false if DLL/symbols missing.
    [[nodiscard]] bool load(QString* error = nullptr);
    void unload();
    [[nodiscard]] bool isLoaded() const { return m_loaded; }
    [[nodiscard]] QString dllPath() const { return m_path; }

    tobii_api_create_fn api_create = nullptr;
    tobii_api_destroy_fn api_destroy = nullptr;
    tobii_error_message_fn error_message = nullptr;
    tobii_get_api_version_fn get_api_version = nullptr;
    tobii_enumerate_local_device_urls_fn enumerate_local_device_urls = nullptr;
    tobii_device_create_fn device_create = nullptr;
    tobii_device_destroy_fn device_destroy = nullptr;
    tobii_wait_for_callbacks_fn wait_for_callbacks = nullptr;
    tobii_device_process_callbacks_fn device_process_callbacks = nullptr;
    tobii_device_clear_callback_buffers_fn device_clear_callback_buffers = nullptr;
    tobii_device_reconnect_fn device_reconnect = nullptr;
    tobii_system_clock_fn system_clock = nullptr;
    tobii_gaze_point_subscribe_fn gaze_point_subscribe = nullptr;
    tobii_gaze_point_unsubscribe_fn gaze_point_unsubscribe = nullptr;
    /// Optional — may be null if DLL export missing.
    tobii_head_pose_subscribe_fn head_pose_subscribe = nullptr;
    tobii_head_pose_unsubscribe_fn head_pose_unsubscribe = nullptr;

    [[nodiscard]] QString errorString(tobii_error_t err) const;

private:
    void* m_handle = nullptr; // HMODULE
    bool m_loaded = false;
    QString m_path;
};

} // namespace gazer

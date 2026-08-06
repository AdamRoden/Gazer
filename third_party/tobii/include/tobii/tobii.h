/* Minimal Stream Engine declarations used by Gazer.
 * Mirrors the public C ABI of tobii_stream_engine.dll (consumer devices).
 * Not a full SDK dump — only symbols we resolve at runtime.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobii_api_t tobii_api_t;
typedef struct tobii_device_t tobii_device_t;

typedef enum tobii_error_t {
    TOBII_ERROR_NO_ERROR,
    TOBII_ERROR_INTERNAL,
    TOBII_ERROR_INSUFFICIENT_LICENSE,
    TOBII_ERROR_NOT_SUPPORTED,
    TOBII_ERROR_NOT_AVAILABLE,
    TOBII_ERROR_CONNECTION_FAILED,
    TOBII_ERROR_TIMED_OUT,
    TOBII_ERROR_ALLOCATION_FAILED,
    TOBII_ERROR_INVALID_PARAMETER,
    TOBII_ERROR_CALIBRATION_ALREADY_STARTED,
    TOBII_ERROR_CALIBRATION_NOT_STARTED,
    TOBII_ERROR_ALREADY_SUBSCRIBED,
    TOBII_ERROR_NOT_SUBSCRIBED,
    TOBII_ERROR_OPERATION_FAILED,
    TOBII_ERROR_CONFLICTING_API_INSTANCES,
    TOBII_ERROR_CALIBRATION_BUSY,
    TOBII_ERROR_CALLBACK_IN_PROGRESS,
    TOBII_ERROR_TOO_MANY_SUBSCRIBERS,
    TOBII_ERROR_CONNECTION_FAILED_DRIVER,
    TOBII_ERROR_UNAUTHORIZED,
    TOBII_ERROR_FIRMWARE_UPGRADE_IN_PROGRESS
} tobii_error_t;

typedef enum tobii_validity_t {
    TOBII_VALIDITY_INVALID = 0,
    TOBII_VALIDITY_VALID = 1
} tobii_validity_t;

typedef enum tobii_field_of_use_t {
    TOBII_FIELD_OF_USE_INTERACTIVE = 1,
    TOBII_FIELD_OF_USE_ANALYTICAL = 2
} tobii_field_of_use_t;

typedef struct tobii_version_t {
    int major;
    int minor;
    int revision;
    int build;
} tobii_version_t;

typedef struct tobii_custom_alloc_t tobii_custom_alloc_t;
typedef struct tobii_custom_log_t tobii_custom_log_t;

typedef void (*tobii_device_url_receiver_t)(char const* url, void* user_data);

/* Function pointer types (resolved via QLibrary / GetProcAddress). */
typedef tobii_error_t (*tobii_api_create_fn)(tobii_api_t** api,
                                             tobii_custom_alloc_t const* custom_alloc,
                                             tobii_custom_log_t const* custom_log);
typedef tobii_error_t (*tobii_api_destroy_fn)(tobii_api_t* api);
typedef char const* (*tobii_error_message_fn)(tobii_error_t error);
typedef tobii_error_t (*tobii_get_api_version_fn)(tobii_version_t* version);
typedef tobii_error_t (*tobii_enumerate_local_device_urls_fn)(
    tobii_api_t* api, tobii_device_url_receiver_t receiver, void* user_data);
typedef tobii_error_t (*tobii_device_create_fn)(tobii_api_t* api,
                                                char const* url,
                                                tobii_field_of_use_t field_of_use,
                                                tobii_device_t** device);
typedef tobii_error_t (*tobii_device_destroy_fn)(tobii_device_t* device);
typedef tobii_error_t (*tobii_wait_for_callbacks_fn)(int device_count,
                                                     tobii_device_t* const* devices);
typedef tobii_error_t (*tobii_device_process_callbacks_fn)(tobii_device_t* device);
typedef tobii_error_t (*tobii_device_clear_callback_buffers_fn)(tobii_device_t* device);
typedef tobii_error_t (*tobii_device_reconnect_fn)(tobii_device_t* device);
typedef tobii_error_t (*tobii_system_clock_fn)(tobii_api_t* api, int64_t* timestamp_us);

#ifdef __cplusplus
}
#endif

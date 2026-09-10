/* Gaze point stream declarations for Stream Engine. */
#pragma once

#include "tobii.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobii_gaze_point_t {
    int64_t timestamp_us;
    tobii_validity_t validity;
    float position_xy[2]; /* normalized [0,1] in active display area */
} tobii_gaze_point_t;

typedef void (*tobii_gaze_point_callback_t)(tobii_gaze_point_t const* gaze_point,
                                           void* user_data);

typedef tobii_error_t (*tobii_gaze_point_subscribe_fn)(tobii_device_t* device,
                                                       tobii_gaze_point_callback_t callback,
                                                       void* user_data);
typedef tobii_error_t (*tobii_gaze_point_unsubscribe_fn)(tobii_device_t* device);

/* Head pose stream (optional — soft-bind at runtime). */
typedef struct tobii_head_pose_t {
    int64_t timestamp_us;
    tobii_validity_t position_validity;
    float position_xyz[3]; /* mm; +X right, +Y up, +Z toward user */
    tobii_validity_t rotation_validity_xyz[3];
    float rotation_xyz[3]; /* radians; X=pitch, Y=yaw, Z=roll */
} tobii_head_pose_t;

typedef void (*tobii_head_pose_callback_t)(tobii_head_pose_t const* head_pose, void* user_data);

typedef tobii_error_t (*tobii_head_pose_subscribe_fn)(tobii_device_t* device,
                                                     tobii_head_pose_callback_t callback,
                                                     void* user_data);
typedef tobii_error_t (*tobii_head_pose_unsubscribe_fn)(tobii_device_t* device);

#ifdef __cplusplus
}
#endif

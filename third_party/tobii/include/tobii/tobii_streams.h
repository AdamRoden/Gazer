/* Gaze point stream declarations for Stream Engine. */
#pragma once

#include "tobii/tobii.h"

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

#ifdef __cplusplus
}
#endif

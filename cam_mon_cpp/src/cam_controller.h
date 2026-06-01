#pragma once

#include <cstdint>
#include "cammon_api.h"

extern "C" {
// Opaque handle to the controller
typedef struct CamController CamController;

// Create/destroy
CAMMON_API CamController* cam_controller_create();
CAMMON_API void cam_controller_destroy(CamController* h);

// Start listening on the given UDP port. Returns 0 on success, negative on error.
// If `mcast_group` is non-null and non-empty, the listener will attempt to
// join the specified multicast group on the given port. Pass NULL or empty
// string to run in point-to-point (direct UDP) mode.
CAMMON_API int cam_controller_start_ex(CamController* h, int port, const char* mcast_group);
// Backwards-compatible wrapper: start without multicast group.
static inline int cam_controller_start(CamController* h, int port) { return cam_controller_start_ex(h, port, nullptr); }
// Stop listening (blocks until thread exits)
CAMMON_API void cam_controller_stop(CamController* h);
// Get last received status packet. Copies up to buflen bytes into buf and returns length, or 0 if none.
CAMMON_API int cam_controller_get_last(CamController* h, uint8_t* buf, int buflen);

// Get parsed PTZ values from the last received status packet.
// Writes available values into the provided pointers. Returns 1 if values are available, 0 otherwise.
CAMMON_API int cam_controller_get_ptz(CamController* h, float* out_az, float* out_el, float* out_ir_focus, float* out_vis_focus);

// Send a PTZ (servo) command to the specified host:port. Wrapper around cammon_send_servo_command.
// Returns the same return code as cammon_send_servo_command (received bytes >0 or negative error code).
CAMMON_API int cam_controller_set_ptz(CamController* h, const char* host, int port,
						  float az, float el, float azs, float els,
						  uint16_t target_distance, uint8_t seq, uint8_t control,
						  uint8_t device_type, uint8_t packet_type,
						  uint8_t* resp_buf, int resp_buf_len, int timeout_ms);
}

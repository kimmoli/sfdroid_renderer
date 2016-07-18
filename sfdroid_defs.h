#ifndef __SFDROID_DEFS_H__
#define __SFDROID_DEFS_H__

#define SFDROID_ROOT "/tmp/sfdroid"
#define SHAREBUFFER_HANDLE_FILE (SFDROID_ROOT "/gralloc_buffer_handle")
#define SENSORS_HANDLE_FILE (SFDROID_ROOT "/sensors_handle")
#define APP_HELPERS_HANDLE_FILE (SFDROID_ROOT "/app_helpers_handle")
#define AM_START_STILL_RUNNING_FILE (SFDROID_ROOT "/to_front_still_processing")

// hmmm
#define MAX_NUM_FDS 32
#define MAX_NUM_INTS 32

#define DUMMY_RENDER_TIMEOUT_MS 250

#define SHAREBUFFER_SOCKET_TIMEOUT_US 250000
#define SHAREBUFFER_SOCKET_FOCUS_LOST_TIMEOUT_S 60*60*24
#define SENSOR_SOCKET_TIMEOUT_US 250000
#define SENSOR_SOCKET_FOCUS_LOST_TIMEOUT_S 60*60*24

#define APP_SOCKET_TIMEOUT_S 60*60*24

#define SLEEPTIME_NO_FOCUS_US 500000

#define SWIPE_HACK_PIXEL_PERCENT 4

#define ACCELEROMETER 0

#include <cstdint>

struct buffer_info_t
{
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t pixel_format;
};

#include <hardware/gralloc.h>
#include <hardware/hardware.h>

int recv_native_handle(int fd, native_handle_t **handle, struct buffer_info_t *info);
int send_status(int fd, int failed);
void free_handle(native_handle_t *handle);

enum sfdroid_event_type
{
    LAYER_NAME = 0,
    LAYER_CLOSE = 1,
    BUFFER = 2,
    NO_BUFFER = 3,
    LAST_WINDOW_CLOSED = 4,
};

#include <system/window.h>

struct sfdroid_event
{
    sfdroid_event_type type;

    union {
        char layer_name[1024];
        struct {
            ANativeWindowBuffer *buffer;
            buffer_info_t *info;
        } buffer;
    } data;
};

#include <vector>
#include <mutex>

extern std::vector<sfdroid_event> sfdroid_events;
extern std::mutex sfdroid_events_mutex;

#endif


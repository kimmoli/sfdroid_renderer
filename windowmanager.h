#ifndef __WINDOWMANAGER_H__
#define __WINDOWMANAGER_H__

#include <map>
#include <string>
#include <vector>
#include <list>

#include <wayland-client.h>

#include "sfdroid_defs.h"
#include "renderer.h"
#include "uinput.h"
#include "wayland_helper.h"
#include "sfconnection.h"

class windowmanager_t {
    public:
        windowmanager_t() : sfconnection(nullptr), w_touch(nullptr), w_keyboard(nullptr), swipe_hack_dist_x(0), swipe_hack_dist_y(0), taken_focus(nullptr), wait_for_next_layer_name(false) {}
        int init(sfconnection_t &sfconnection);
        void deinit();

        static void seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t caps);

        static void touch_handle_down(void *data, struct wl_touch *wl_touch, uint32_t serial, uint32_t time, struct wl_surface *surface, int32_t id, wl_fixed_t x, wl_fixed_t y);
        static void touch_handle_up(void *data, struct wl_touch *wl_touch, uint32_t serial, uint32_t time, int32_t id);
        static void touch_handle_motion(void *data, struct wl_touch *wl_touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y);
        static void touch_handle_frame(void *data, struct wl_touch *wl_touch);
        static void touch_handle_cancel(void *data, struct wl_touch *wl_touch);

        static void keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size);
        static void keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
        static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface);
        static void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
        static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
        static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay);

        void handle_layer_name_event(char *layer_name);
        void handle_layer_close_event(char *layer_name);
        bool handle_buffer_event(ANativeWindowBuffer *buffer, buffer_info_t &info);
        bool handle_no_buffer_event(ANativeWindowBuffer *old_buffer, buffer_info_t &info);
        void handle_close(struct wl_surface *surface);

        const struct wl_seat_listener w_seat_listener = {
            seat_handle_capabilities,
        };

    private:
        void take_focus();
        sfconnection_t *sfconnection;
        wl_touch *w_touch;
        wl_keyboard *w_keyboard;

        uinput_t uinput;

        std::map<std::string, renderer_t*> windows;
        std::vector<int> slot_to_fingerId;
        int swipe_hack_dist_x;
        int swipe_hack_dist_y;

        const struct wl_touch_listener w_touch_listener = {
            touch_handle_down,
            touch_handle_up,
            touch_handle_motion,
            touch_handle_frame,
            touch_handle_cancel,
        };

        const struct wl_keyboard_listener w_keyboard_listener = {
            keyboard_handle_keymap,
            keyboard_handle_enter,
            keyboard_handle_leave,
            keyboard_handle_key,
            keyboard_handle_modifiers,
            keyboard_handle_repeat_info,
        };

        renderer_t *taken_focus;
        bool wait_for_next_layer_name;
        std::string last_layer;
};

#endif


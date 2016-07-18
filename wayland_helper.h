#ifndef __WAYLAND_HELPER_H__
#define __WAYLAND_HELPER_H__

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>

#include "windowmanager.h"

struct qt_surface_extension;

class wayland_helper {
    public:
        static int init(windowmanager_t &windowmanager);
        static void dispatch();
        static void roundtrip();
        static void deinit();

    private:
        static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name);
        static void registry_add_object(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);

        static void output_handle_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform);
        static void output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);
        static void output_handle_done(void *data, struct wl_output *wl_output);
        static void output_handle_scale(void *data, struct wl_output *wl_output, int32_t factor);

    public:
        static EGLDisplay egl_display;

        static struct wl_display *display;
        static struct wl_compositor *compositor;
        static struct wl_shell *shell;
        static struct wl_seat *seat;
        static struct wl_registry *registry;
        static const struct wl_registry_listener registry_listener;
        static struct wl_output *output;
        static const struct wl_output_listener output_listener;
        static struct android_wlegl *a_android_wlegl;

        static int32_t width;
        static int32_t height;

        static windowmanager_t *windowmanager;

        static const struct wl_interface *qt_surface_extension_types[4];
        static const struct wl_message qt_surface_extension_requests[1];
        static const struct wl_interface qt_surface_extension_interface;
        static const struct wl_message qt_extended_surface_requests[3];
        static const struct wl_message qt_extended_surface_events[3];
        static const struct wl_interface qt_extended_surface_interface;
        static struct qt_surface_extension *q_surface_extension;
};

#endif


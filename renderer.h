#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <GLES/gl.h>

#include "sfdroid_defs.h"

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <system/window.h>
#include <string>
#include <map>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/glext.h>

#include <wayland-client.h>

class windowmanager_t;

struct qt_extended_surface_listener {
    void (*onscreen_visibility)(void *data, struct qt_extended_surface *qt_extended_surface, int32_t visible);
    void (*set_generic_property)(void *data, struct qt_extended_surface *qt_extended_surface, const char *name, struct wl_array *value);
    void (*close)(void *data, struct qt_extended_surface *qt_extended_surface);
};

class renderer_t {
    public:
        renderer_t() : have_focus(0), last_screen(nullptr), egl_surf(EGL_NO_SURFACE), egl_ctx(EGL_NO_CONTEXT), w_shell_surface(nullptr), w_surface(nullptr), w_egl_window(nullptr), buffer(nullptr), windowmanager(nullptr) { }
        int init(windowmanager_t &wm);
        int recreate();
        int render_buffer(ANativeWindowBuffer *the_buffer, buffer_info_t &info);
        void gained_focus();
        wl_surface *get_surface() { return w_surface; }
        void lost_focus();
        bool is_active();
        void deinit();
        int save_screen();
        int dummy_draw(int stride, int height, int format);
        void set_package(std::string pack) { app = pack; }
        std::string get_package() { return app; }
        ~renderer_t();

    private:
        static void shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial);
        static void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height);
        static void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface);

        GLuint dummy_tex;
        bool have_focus;

        int draw_raw(void *data, int width, int height, int pixel_format);
        void *last_screen;

        EGLSurface egl_surf;
        EGLContext egl_ctx;

        struct wl_shell_surface *w_shell_surface;
        struct wl_surface *w_surface;
        struct wl_egl_window *w_egl_window;
        struct wl_shell_surface_listener shell_surface_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};

        struct qt_extended_surface *q_extended_surface;

        static int instances;

        std::string app;

        ANativeWindowBuffer *buffer;

        static void handle_onscreen_visibility(void *data, struct qt_extended_surface *qt_extended_surface, int32_t visible);
        static void handle_set_generic_property(void *data, struct qt_extended_surface *qt_extended_surface, const char *name, struct wl_array *value);
        static void handle_close(void *data, struct qt_extended_surface *qt_extended_surface);

        static void buffer_release(void *data, struct wl_buffer *buffer);

        static void frame_callback(void *data, struct wl_callback *callback, uint32_t time);

        const struct qt_extended_surface_listener extended_surface_listener = { 
            &handle_onscreen_visibility,
            &handle_set_generic_property,
            &handle_close,
        };

        const struct wl_buffer_listener w_buffer_listener = {
            buffer_release
        };

        const struct wl_callback_listener w_frame_listener = {
            frame_callback
        };

        struct wl_callback *frame_callback_ptr;

        std::map<ANativeWindowBuffer*, struct wl_buffer*> buffer_map;
        windowmanager_t *windowmanager;
};

#include "windowmanager.h"

#endif


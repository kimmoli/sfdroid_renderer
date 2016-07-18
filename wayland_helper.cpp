/*
 *  this file is part of sfdroid
 *  Copyright (C) 2015, Franz-Josef Haider <f_haider@gmx.at>
 *  based on harmattandroid by Thomas Perl
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "wayland_helper.h"

#include <iostream>
#include <cstring>

#include <poll.h>

#include "wayland-android-client-protocol.h"

using namespace std;

struct wl_display *wayland_helper::display(nullptr);
struct wl_compositor *wayland_helper::compositor(nullptr);
struct wl_shell *wayland_helper::shell(nullptr);
EGLDisplay wayland_helper::egl_display(EGL_NO_DISPLAY);
struct wl_registry *wayland_helper::registry(nullptr);
const struct wl_registry_listener wayland_helper::registry_listener = {&wayland_helper::registry_add_object, &wayland_helper::registry_remove_object};
const struct wl_output_listener wayland_helper::output_listener = {&wayland_helper::output_handle_geometry, &wayland_helper::output_handle_mode, &wayland_helper::output_handle_done, &wayland_helper::output_handle_scale};
int32_t wayland_helper::width;
int32_t wayland_helper::height;
struct wl_output *wayland_helper::output;
struct wl_seat *wayland_helper::seat;
struct windowmanager_t *wayland_helper::windowmanager;
struct qt_surface_extension *wayland_helper::q_surface_extension;
struct android_wlegl *wayland_helper::a_android_wlegl;

int wayland_helper::init(windowmanager_t &wm)
{
    windowmanager = &wm;

    display = wl_display_connect(0);
    if(!display) return 1;

    registry = wl_display_get_registry(display);
    if(!registry) return 2;

    wl_registry_add_listener(registry, &registry_listener, 0);

    wl_display_roundtrip(display);

    wl_output_add_listener(output, &output_listener, 0);
    wl_seat_add_listener(seat, &windowmanager->w_seat_listener, windowmanager);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

#if DEBUG
    cout << "window width: " << width << " height: " << height << endl;
#endif

    egl_display = eglGetDisplay(display);
    if(egl_display == EGL_NO_DISPLAY) return 3;

    eglInitialize(egl_display, nullptr, nullptr);

    return 0;
}

void wayland_helper::dispatch()
{
    struct pollfd pfd[1];

    pfd[0].fd = wl_display_get_fd(display);
    pfd[0].events = POLLIN;
    poll(pfd, 1, 0);

    if (pfd[0].revents & POLLIN)
        wl_display_dispatch(display);
    else
        wl_display_dispatch_pending(display);
}

void wayland_helper::roundtrip()
{
    wl_display_roundtrip(display);
}

void wayland_helper::registry_add_object(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if(strcmp(interface, "wl_compositor") == 0)
    {
        compositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 0);
    }
    else if(strcmp(interface, "wl_shell") == 0)
    {
        shell = (struct wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 0);
    }
    else if(strcmp(interface, "wl_output") == 0)
    {
        output = (struct wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 0);
    }
    else if(strcmp(interface, "wl_seat") == 0)
    {
        seat = (struct wl_seat*)wl_registry_bind(registry, name, &wl_seat_interface, 0);
    }
    else if(strcmp(interface, "qt_surface_extension") == 0)
    {
        q_surface_extension = (struct qt_surface_extension*)wl_registry_bind(registry, name, &wayland_helper::qt_surface_extension_interface, 0);
    }
    else if(strcmp(interface, "android_wlegl") == 0)
    {
        a_android_wlegl = static_cast<struct android_wlegl*>(wl_registry_bind(registry, name, &android_wlegl_interface, 1));
    }
    else
    {
#if DEBUG
        cout << "ignored interface: " << interface << endl;
#endif
    }
}

void wayland_helper::registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
#if DEBUG
    cout << "registry remove object" << endl;
#endif
}

void wayland_helper::output_handle_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{
#if DEBUG
    cout << "output handle geometry" << endl;
#endif
}

void wayland_helper::output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
#if DEBUG
    cout << "output handle mode" << endl;
#endif
    wayland_helper::width = width;
    wayland_helper::height = height;
}

void wayland_helper::output_handle_done(void *data, struct wl_output *wl_output)
{
#if DEBUG
    cout << "output handle done" << endl;
#endif
}

void wayland_helper::output_handle_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
#if DEBUG
    cout << "output handle scale" << endl;
#endif
}

void wayland_helper::deinit()
{
    eglTerminate(egl_display);
    android_wlegl_destroy(a_android_wlegl);
    wl_display_disconnect(display);
}

const struct wl_message wayland_helper::qt_extended_surface_requests[3] = {
    { "update_generic_property", "sa", wayland_helper::qt_surface_extension_types + 0 },
    { "set_content_orientation", "i", wayland_helper::qt_surface_extension_types + 0 },
    { "set_window_flags", "i", wayland_helper::qt_surface_extension_types + 0 },
};

const struct wl_message wayland_helper::qt_extended_surface_events[3] = {
    { "onscreen_visibility", "i", wayland_helper::qt_surface_extension_types + 0 },
    { "set_generic_property", "sa", wayland_helper::qt_surface_extension_types + 0 },
    { "close", "", wayland_helper::qt_surface_extension_types + 0 },
};

WL_EXPORT const struct wl_interface wayland_helper::qt_extended_surface_interface = {
    "qt_extended_surface", 1,
    3, wayland_helper::qt_extended_surface_requests,
    3, wayland_helper::qt_extended_surface_events,
};
const struct wl_interface *wayland_helper::qt_surface_extension_types[4] = {
    NULL,
    NULL,
    &wayland_helper::qt_extended_surface_interface,
    &wl_surface_interface,
};

const struct wl_message wayland_helper::qt_surface_extension_requests[1] = {
    { "get_extended_surface", "no", wayland_helper::qt_surface_extension_types + 2 },
};

WL_EXPORT const struct wl_interface wayland_helper::qt_surface_extension_interface = {
    "qt_surface_extension", 1,
    1, wayland_helper::qt_surface_extension_requests,
    0, NULL,
};


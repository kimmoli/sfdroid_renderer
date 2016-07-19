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

#include <iostream>

#include <GLES/gl.h>
#include <wayland-egl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "wayland-android-client-protocol.h"

#include "renderer.h"
#include "wayland_helper.h"
#include "sfconnection.h"

using namespace std;

#define QT_SURFACE_EXTENSION_GET_EXTENDED_SURFACE 0

int renderer_t::instances(0);

int renderer_t::init(windowmanager_t &wm)
{
    int err = 0;

    frame_callback_ptr = 0;

    GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };
    EGLConfig egl_cfg;
    EGLint contextParams[] = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE};
    EGLint numConfigs;

    windowmanager = &wm;

#if DEBUG
    cout << "choosing egl config" << endl;
#endif
    if(eglChooseConfig(wayland_helper::egl_display, configAttribs, &egl_cfg, 1, &numConfigs) != EGL_TRUE || numConfigs != 1)
    {
        cerr << "unable to find an EGL Config" << endl;
        err = 7;
        goto quit;
    }

#if DEBUG
    cout << "creating GLES context" << endl;
#endif
    eglBindAPI(EGL_OPENGL_ES_API);
    egl_ctx = eglCreateContext(wayland_helper::egl_display, egl_cfg, NULL, contextParams);
    if(egl_ctx == EGL_NO_CONTEXT)
    {
        cerr << "unable to create GLES context" << endl;
        err = 9;
        goto quit;
    }

#if DEBUG
    cout << "creating surface" << endl;
#endif
    w_surface = wl_compositor_create_surface(wayland_helper::compositor);

#if DEBUG
    cout << "creating shell surface" << endl;
#endif
    w_shell_surface = wl_shell_get_shell_surface(wayland_helper::shell, w_surface);

#if DEBUG
    cout << "getting qt extended surface" << endl;
#endif

    q_extended_surface = (struct qt_extended_surface*)wl_proxy_create((struct wl_proxy *)wayland_helper::q_surface_extension, &wayland_helper::qt_extended_surface_interface);
    if(!q_extended_surface)
    {
        err = 17;
        goto quit;
    }

    wl_proxy_marshal((struct wl_proxy*)wayland_helper::q_surface_extension, QT_SURFACE_EXTENSION_GET_EXTENDED_SURFACE, q_extended_surface, w_surface);

    wl_proxy_add_listener((struct wl_proxy*)q_extended_surface, (void (**)(void))&extended_surface_listener, this);
    wayland_helper::roundtrip();

    wl_shell_surface_add_listener(w_shell_surface, &shell_surface_listener, NULL);

    wl_shell_surface_set_toplevel(w_shell_surface);

    struct wl_region *region;
    region = wl_compositor_create_region(wayland_helper::compositor);
    wl_region_add(region, 0, 0,
                  wayland_helper::width,
                  wayland_helper::height);
    wl_surface_set_opaque_region(w_surface, region);
    wl_region_destroy(region);

#if DEBUG
    cout << "creating wl egl window" << endl;
#endif
    w_egl_window = wl_egl_window_create(w_surface, wayland_helper::width, wayland_helper::height);
    if(w_egl_window == NULL)
    {
        cerr << "unable to create an egl window" << endl;
        err = 16;
        goto quit;
    }

#if DEBUG
    cout << "creating egl window surface" << endl;
#endif
    egl_surf = eglCreateWindowSurface(wayland_helper::egl_display, egl_cfg, (EGLNativeWindowType)w_egl_window, 0);
    if(egl_surf == EGL_NO_SURFACE)
    {
        cerr << "unable to create an EGLSurface" << endl;
        err = 8;
        goto quit;
    }

#if DEBUG
    cout << "making GLES context current" << endl;
#endif
    if(eglMakeCurrent(wayland_helper::egl_display, egl_surf, egl_surf, egl_ctx) == EGL_FALSE)
    {
        cerr << "unable to make GLES context current" << endl;
        err = 10;
        goto quit;
    }

#if DEBUG
    cout << "setting up gl" << endl;
#endif
    glViewport(0, 0, wayland_helper::width, wayland_helper::height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, wayland_helper::width, wayland_helper::height, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.f, 1.f, 1.f, 1.f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glGenTextures(1, &dummy_tex);
    glBindTexture(GL_TEXTURE_2D, dummy_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if DEBUG
    // this might even crash if the patch is missing:
    cout << "WARNING: if i crash now the patch is missing, look at " << __FILE__ << "@" << __LINE__ << endl;
    if(glGetString(GL_VERSION) == 0)
    {
        cout << "oh no, gles v1 not working, do you have this patch: https://github.com/mer-hybris/android_frameworks_native/pull/3/files ?" << endl;
        err = 3843;
        goto quit;
    }
#endif

quit:
    return err;
}

void renderer_t::deinit()
{
    for(map<ANativeWindowBuffer*, struct wl_buffer*>::iterator it = buffer_map.begin();it != buffer_map.end();it++)
    {
        wl_buffer_destroy(it->second);
    }
    buffer_map.clear();

    have_focus = false;
    glDeleteTextures(1, &dummy_tex);
    eglMakeCurrent(wayland_helper::egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(wayland_helper::egl_display, egl_surf);
    wl_egl_window_destroy(w_egl_window);
    wl_proxy_destroy((struct wl_proxy *)q_extended_surface);
    wl_shell_surface_destroy(w_shell_surface);
    wl_surface_destroy(w_surface);
    eglDestroyContext(wayland_helper::egl_display, egl_ctx);
}

renderer_t::~renderer_t()
{
    instances--;
}

int renderer_t::draw_raw(void *data, int width, int height, int pixel_format)
{
#if DEBUG
    cout << "draw raw: " << width << " " << height << " " << pixel_format << endl;
#endif
    int err = 0;

    GLuint gl_err = 0;

    float xf = (float)wayland_helper::width / (float)width;
    float yf = 1.f;
    float texcoords[] = {
        0.f, 0.f,
        xf, 0.f,
        0.f, yf,
        xf, yf,
    };

    float vtxcoords[] = {
        0.f, 0.f,
        (float)wayland_helper::width, 0.f,
        0.f, (float)wayland_helper::height,
        (float)wayland_helper::width, (float)wayland_helper::height,
    };

    glVertexPointer(2, GL_FLOAT, 0, &vtxcoords);
    glTexCoordPointer(2, GL_FLOAT, 0, &texcoords);

    glBindTexture(GL_TEXTURE_2D, dummy_tex);

    if(pixel_format == HAL_PIXEL_FORMAT_RGBA_8888 || pixel_format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    else if(pixel_format == HAL_PIXEL_FORMAT_RGB_565)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
    }
    else
    {
        cerr << "unhandled pixel format: " << pixel_format << endl;
        err = 3;
        goto quit;
    }
    gl_err = glGetError();
    if(gl_err != GL_NO_ERROR) cerr << "glGetError(): " << gl_err << endl;

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(wayland_helper::egl_display, egl_surf);

quit:
    return err;
}

int renderer_t::save_screen()
{
    int err = 0;
    int gerr = 0;
    void *buffer_vaddr;

    if(last_screen) return 0;

#if DEBUG
    cout << "saving screen" << endl;
#endif

    gerr = gralloc_module->lock(gralloc_module, buffer->handle,
        GRALLOC_USAGE_SW_READ_RARELY,
        0, 0, buffer->width, buffer->height,
        &buffer_vaddr);

    if(gerr)
    {
        cerr << "gralloc lock failed" << endl;
        err = 3;
        goto quit;
    }

    if(last_screen) free(last_screen);
    last_screen = nullptr;

    if(buffer->format == HAL_PIXEL_FORMAT_RGBA_8888 || buffer->format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        last_screen = (GLubyte*)malloc(4 * buffer->stride * buffer->height);
        memcpy(last_screen, buffer_vaddr, 4 * buffer->stride * buffer->height);
    }
    else if(buffer->format == HAL_PIXEL_FORMAT_RGB_565)
    {
        last_screen = (GLubyte*)malloc(4 * buffer->stride * buffer->height);
        memcpy(last_screen, buffer_vaddr, 4 * buffer->stride * buffer->height);
    }
    else
    {
        cerr << "unhandled pixel format: " << buffer->format << endl;
        err = 1;
        goto quit;
    }

    gralloc_module->unlock(gralloc_module, buffer->handle);

quit:
    return err;
}

int renderer_t::dummy_draw(int stride, int height, int format)
{
#if DEBUG
    cout << "dummy draw" << endl;
#endif

    if(last_screen != nullptr)
    {
        draw_raw(last_screen, stride, height, format);
        free(last_screen);
        last_screen = nullptr;
    }

    return -1;
}

void renderer_t::lost_focus()
{
#if DEBUG
    cout << "losing focus: " << app << endl;
#endif
    // TODO: check if still necessary
    if(!is_active())
    {
        cout << "not active" << endl;
        return;
    }
    else if(save_screen() == 0)
    {
        dummy_draw(buffer->stride, buffer->height, buffer->format);
    }

    eglMakeCurrent(wayland_helper::egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    have_focus = false;
}

void renderer_t::gained_focus()
{
    wl_shell_surface_set_toplevel(w_shell_surface);
    eglMakeCurrent(wayland_helper::egl_display, egl_surf, egl_surf, egl_ctx);
    have_focus = true;
}

int renderer_t::recreate()
{
    int ret;
    deinit();
    ret = init(*windowmanager);
    have_focus = true;
    return ret;
}

bool renderer_t::is_active()
{
    return have_focus;
}

int renderer_t::render_buffer(ANativeWindowBuffer *the_buffer, buffer_info_t &info)
{
#if DEBUG
    cout << "rendering buffer in: " << app << endl;
#endif
    buffer = the_buffer;

    if(buffer_map.find(the_buffer) == buffer_map.end())
    {
        struct wl_buffer *w_buffer;
        struct wl_array ints;
        int *the_ints;
        struct android_wlegl_handle *wlegl_handle;

        wl_array_init(&ints);
        the_ints = (int*)wl_array_add(&ints, buffer->handle->numInts * sizeof(int));
        memcpy(the_ints, buffer->handle->data + buffer->handle->numFds, buffer->handle->numInts * sizeof(int));
        wlegl_handle = android_wlegl_create_handle(wayland_helper::a_android_wlegl, buffer->handle->numFds, &ints);
        wl_array_release(&ints);

        for (int i = 0; i < buffer->handle->numFds; i++)
        {
            android_wlegl_handle_add_fd(wlegl_handle, buffer->handle->data[i]);
        }

        w_buffer = android_wlegl_create_buffer(wayland_helper::a_android_wlegl, info.width, info.height, info.stride, info.pixel_format, GRALLOC_USAGE_HW_RENDER, wlegl_handle);
        android_wlegl_handle_destroy(wlegl_handle);

        wl_buffer_add_listener(w_buffer, &w_buffer_listener, this);

        buffer_map[the_buffer] = w_buffer;
    }

    int ret = 0;
    while(frame_callback_ptr && ret != -1)
    {
        ret = wl_display_dispatch(wayland_helper::display);
    }

    if(!have_focus)
    {
        // lost focus due to keyboard leave
        return 0;
    }

    frame_callback_ptr = wl_surface_frame(w_surface);
    wl_callback_add_listener(frame_callback_ptr, &w_frame_listener, this);

    wl_surface_attach(w_surface, buffer_map[the_buffer], 0, 0);
    wl_surface_damage(w_surface, 0, 0, info.width, info.height);
    wl_surface_commit(w_surface);

    if(last_screen) free(last_screen);
    last_screen = nullptr;

    return 0;
}

void renderer_t::buffer_release(void *data, struct wl_buffer *buffer)
{
#if DEBUG
    cout << "buffer release" << endl;
#endif

    // we're cleaning in deinit()
}

void renderer_t::shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
#if DEBUG
    cout << "shell surface ping " << endl;
#endif
    wl_shell_surface_pong(shell_surface, serial);
}

void renderer_t::shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
#if DEBUG
    cout << "shell surface configure " << endl;
#endif
    renderer_t *self = (renderer_t*)data;
    wl_egl_window_resize(self->w_egl_window, width, height, 0, 0);
}

void renderer_t::shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
#if DEBUG
    cout << "shell surface popup done" << endl;
#endif
}

void renderer_t::handle_onscreen_visibility(void *data, struct qt_extended_surface *qt_extended_surface, int32_t visible)
{
#if DEBUG
    renderer_t *renderer = (renderer_t*)data;
    cout << "qt_extended_surface handle onscreen visibility @ "<< renderer->app << endl;
#endif
    // handled in keyboard leave/enter
}

void renderer_t::handle_set_generic_property(void *data, struct qt_extended_surface *qt_extended_surface, const char *name, struct wl_array *value)
{
#if DEBUG
    cout << "qt_extended_surface handle set generic property" << endl;
#endif
}

void renderer_t::handle_close(void *data, struct qt_extended_surface *qt_extended_surface)
{
#if DEBUG
    cout << "qt_extended_surface handle close" << endl;
#endif

    renderer_t *renderer = (renderer_t*)data;
    renderer->windowmanager->handle_close(renderer->w_surface);
}

void renderer_t::frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    renderer_t *renderer = (renderer_t*)data;
    renderer->frame_callback_ptr = 0;
    wl_callback_destroy(callback);
}


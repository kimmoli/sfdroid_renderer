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
#include <chrono>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "sfconnection.h"
#include "utility.h"

using namespace std;

gralloc_module_t *gralloc_module(nullptr);

int sfconnection_t::init()
{
    int err = 0;
    struct sockaddr_un addr;

    fd_pass_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd_pass_socket < 0)
    {
        cerr << "failed to create socket: " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SHAREBUFFER_HANDLE_FILE, sizeof(addr.sun_path)-1);

    unlink(SHAREBUFFER_HANDLE_FILE);

    if(bind(fd_pass_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        cerr << "failed to bind socket" << SHAREBUFFER_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 2;
        goto quit;
    }

#if DEBUG
    cout << "listening on " << SHAREBUFFER_HANDLE_FILE << endl;
#endif
    if(listen(fd_pass_socket, 5) < 0)
    {
        cerr << "failed to listen on socket " << SHAREBUFFER_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 3;
        goto quit;
    }

#if DEBUG
    cout << "loading gralloc module" << endl;
#endif
    if(hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&gralloc_module) != 0)
    {
        cerr << "failed to open " << GRALLOC_HARDWARE_MODULE_ID << " module" << endl;
        err = 4;
        goto quit;
    }

    chmod(SHAREBUFFER_HANDLE_FILE, 0770);

quit:
    return err;
}

void dummy_f(android_native_base_t *base)
{
}

int sfconnection_t::wait_for_buffer(int &timedout, bool &is_not_a_buffer)
{
    int err = 0;
    int r;
    int gerr;
    char buf[1];
    ANativeWindowBuffer *buffer = nullptr;
    native_handle_t *handle = nullptr;
    int registered = 0;
    timedout = 0;

#if DEBUG
    cout << "waiting for notification" << endl;
#endif
    r = recv(fd_client, buf, 1, 0);
    if(r < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "lost client " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    if(buf[0] != 0xFF && buf[0] != 0xFE && buf[0] != 0xFD)
    {
#if DEBUG
        cout << "received post notification" << endl;
#endif
        unsigned int index = buf[0];
        if(index < 0 || index > buffers.size())
        {
            cerr << "invalid index: " << index << endl;
            err = 1;
            goto quit;
        }

        current_buffer = buffers[index];
        current_info = buffer_infos[index];

        err = 0;
        goto quit;
    }

    if(buf[0] == 0xFD)
    {
#if DEBUG
        cout << "received close event" << endl;
#endif
        sfdroid_event event;

        r = recv(fd_client, buf, 1, 0);
        if(r < 0)
        {
            if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
            {
                timedout = 1;
                err = 0;
                goto quit;
            }

            cerr << "lost client " << strerror(errno) << endl;
            err = 1;
            goto quit;
        }

        r = recv(fd_client, event.data.layer_name, buf[0], 0);
        if(r < 0)
        {
            if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
            {
                timedout = 1;
                err = 0;
                goto quit;
            }

            cerr << "lost client " << strerror(errno) << endl;
            err = 1;
            goto quit;
        }

        event.data.layer_name[(unsigned int)buf[0]] = 0;

        event.type = LAYER_CLOSE;
        sfdroid_events_mutex.lock();
        sfdroid_events.push_back(event);
        sfdroid_events_mutex.unlock();

        is_not_a_buffer = true;
        err = 0;
        goto quit;
    }

    if(buf[0] == 0xFE)
    {
#if DEBUG
        cout << "received layer name" << endl;
#endif
        sfdroid_event event;

        r = recv(fd_client, buf, 1, 0);
        if(r < 0)
        {
            if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
            {
                timedout = 1;
                err = 0;
                goto quit;
            }

            cerr << "lost client " << strerror(errno) << endl;
            err = 1;
            goto quit;
        }

        r = recv(fd_client, event.data.layer_name, buf[0], 0);
        if(r < 0)
        {
            if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
            {
                timedout = 1;
                err = 0;
                goto quit;
            }

            cerr << "lost client " << strerror(errno) << endl;
            err = 1;
            goto quit;
        }

        event.data.layer_name[(unsigned int)buf[0]] = 0;

        event.type = LAYER_NAME;
        sfdroid_events_mutex.lock();
        sfdroid_events.push_back(event);
        sfdroid_events_mutex.unlock();

        is_not_a_buffer = true;
        err = 0;
        goto quit;
    }

    buffer = new ANativeWindowBuffer();

#if DEBUG
    cout << "waiting for handle" << endl;
#endif
    r = recv_native_handle(fd_client, &handle, &current_info);
    if(r < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "lost client " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }
 
    gerr = gralloc_module->registerBuffer(gralloc_module, handle);
    if(gerr)
    {
        cerr << "registerBuffer failed: " << strerror(-gerr) << endl;
        err = 1;
        goto quit;
    }

    buffer->width = current_info.width;
    buffer->height = current_info.height;
    buffer->stride = current_info.stride;
    buffer->format = current_info.pixel_format;
    buffer->handle = handle;
    buffer->common.incRef = dummy_f;
    buffer->common.decRef = dummy_f;

    current_buffer = buffer;

    buffers.push_back(current_buffer);
    buffer_infos.push_back(current_info);

#if DEBUG
    cout << "buffer info:" << endl;
    cout << "width: " << current_info.width << " height: " << current_info.height << " stride: " << current_info.stride << " pixel_format: " << current_info.pixel_format << endl;
#endif

    is_not_a_buffer = false;

quit:
    if(err != 0)
    {
        if(registered)
        {
            gralloc_module->unregisterBuffer(gralloc_module, handle);
        }
        // does this also make sense if layer name or layer close failed?
        close(fd_client);
        fd_client = -1;
        remove_buffers();
        if(buffer) delete buffer;
        if(handle)
        {
            free_handle(handle);
        }
    }
    return err;
}

void sfconnection_t::remove_buffers()
{
    for(std::vector<ANativeWindowBuffer*>::size_type i = 0;i < buffers.size();i++)
    {
        ANativeWindowBuffer *buffer = buffers[i];
        const native_handle_t *handle = buffer->handle;

        gralloc_module->unregisterBuffer(gralloc_module, handle);

        for(int i=0;i<handle->numFds;i++)
        {
            close(handle->data[i]);
        }
        free((void*)handle);

        delete buffer;
    }

    buffers.resize(0);
    buffer_infos.resize(0);
}

void sfconnection_t::send_status_and_cleanup()
{
    if(fd_client >= 0)
    {
#if DEBUG
        cout << "sending status" << endl;
#endif
        if(send_status(fd_client, current_status) < 0)
        {
            cerr << "lost client" << endl;
            close(fd_client);
            fd_client = -1;
            remove_buffers();
        }
    }
}

buffer_info_t *sfconnection_t::get_current_info()
{
    return &current_info;
}

ANativeWindowBuffer *sfconnection_t::get_current_buffer()
{
    return current_buffer;
}

int sfconnection_t::wait_for_client()
{
    int err = 0;

#if DEBUG
    cout << "waiting for client (sharebuffer module)" << endl;
#endif
    if((fd_client = accept(fd_pass_socket, NULL, NULL)) < 0)
    {
        cerr << "failed to accept: " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    update_timeout();

quit:
    return err;
}

void sfconnection_t::update_timeout()
{
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));

    if(my_have_focus)
    {
        timeout.tv_usec = SHAREBUFFER_SOCKET_TIMEOUT_US;
    }
    else
    {
        timeout.tv_sec = SHAREBUFFER_SOCKET_FOCUS_LOST_TIMEOUT_S;
    }

    if(setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        cerr << "failed to set timeout on sharebuffer socket: " << strerror(errno) << endl;
    }
}

void sfconnection_t::thread_loop()
{
    running = true;

    while(running)
    {
        if(have_client()) update_timeout();

        if(!have_client())
        {
#if DEBUG
            cout << "waking up android" << endl;
#endif
            wakeup_android();
            if(wait_for_client() != 0)
            {
                cerr << "waiting for client failed" << endl;
            }
#if DEBUG
            else
            {
                cout << "new client" << endl;
            }
#endif
        }

        if(have_client())
        {
            int timedout = 0;
            bool is_not_a_buffer = false;
            if(wait_for_buffer(timedout, is_not_a_buffer) == 0)
            {
                back_cond.notify_one();
                if(!is_not_a_buffer)
                {
                    if(!timedout)
                    {
                        sfdroid_event event;
                        event.type = BUFFER;
                        event.data.buffer.buffer = current_buffer;
                        event.data.buffer.info = &current_info;
                        sfdroid_events_mutex.lock();
                        sfdroid_events.push_back(event);
                        sfdroid_events_mutex.unlock();

                        unique_lock<mutex> lock(notify_mutex);

                        buffer_cond.wait(lock);

                        // let sharebuffer know were done
                        send_status_and_cleanup();

                        timeout_count = 0;
                    }
                    else
                    {
                        if(((timeout_count + 1) * SHAREBUFFER_SOCKET_TIMEOUT_US) / 1000 >= DUMMY_RENDER_TIMEOUT_MS)
                        {
                            if(buffers.size() > 0)
                            {
                                sfdroid_event event;
                                event.type = NO_BUFFER;
                                event.data.buffer.buffer = current_buffer;
                                event.data.buffer.info = &current_info;
                                sfdroid_events_mutex.lock();
                                sfdroid_events.push_back(event);
                                sfdroid_events_mutex.unlock();

                                unique_lock<mutex> lock(notify_mutex);

                                buffer_cond.wait(lock);
                            }

                            timeout_count = 0;
                        }
                        else timeout_count++;

                        if(my_have_focus)
                        {
#if DEBUG
                            cout << "wakeing up android" << endl;
#endif
                            wakeup_android();
                        }
                    }
                }
            }
        }

        std::this_thread::yield();
    }
    thread_exited = true;
}

void sfconnection_t::notify_buffer_done(int failed)
{
    current_status = failed;
    buffer_cond.notify_one();
}

void sfconnection_t::start_thread()
{
    thread_exited = false;
    my_thread = std::thread(&sfconnection_t::thread_loop, this);
}

void sfconnection_t::stop_thread()
{
    running = false;
    if(fd_client >= 0) shutdown(fd_client, SHUT_RDWR);
    if(fd_pass_socket >= 0) shutdown(fd_pass_socket, SHUT_RDWR);
    // TODO
    while(!thread_exited)
    {
        usleep(100);
        buffer_cond.notify_one();
    }
    my_thread.join();
}

void sfconnection_t::wait_for_event(int timeout)
{
    unique_lock<mutex> notify_back_lock(notify_back_mutex);
    back_cond.wait_for(notify_back_lock, std::chrono::milliseconds(timeout));
}

bool sfconnection_t::have_client()
{
    return (fd_client >= 0);
}

void sfconnection_t::lost_focus()
{
    my_have_focus = false;
}

void sfconnection_t::gained_focus()
{
    my_have_focus = true;
}

void sfconnection_t::deinit()
{
    remove_buffers();
    if(fd_pass_socket >= 0) close(fd_pass_socket);
    if(fd_client >= 0) close(fd_client);
    unlink(SHAREBUFFER_HANDLE_FILE);
}


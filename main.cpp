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
#include <vector>
#include <algorithm>

#include <wayland-client-protocol.h>

#include <signal.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "sfconnection.h"
#include "sensorconnection.h"
#include "wayland_helper.h"
#include "windowmanager.h"
#include "utility.h"

using namespace std;

std::vector<sfdroid_event> sfdroid_events;
std::mutex sfdroid_events_mutex;

#define MAX_FPS 60
#define SLEEP_MARGIN 0

void usage(const char *name)
{
    cout << name << " [--multiwindow|-m]" << endl;
    cout << "\t--multiwindow|-m android apps get their own windows" << endl;
}

bool running = true;

void sigint_handler(int param)
{
    running = false;
}

int main(int argc, char *argv[])
{
    int err = 0;
    bool multiwindow = false;

    sfconnection_t sfconnection;
    windowmanager_t windowmanager;
    sensorconnection_t sensorconnection;

    struct timeval current_time, old_time;
    int frames = 0, failed_frames = 0, dummy_frames = 0, failed_dummy_frames = 0;

    signal(SIGINT, sigint_handler);

    if(argc == 2)
    {
        if((std::string)argv[1] == "--multiwindow" || (std::string)argv[1] == "-m")
        {
            multiwindow = true;
        }
        else
        {
            cout << "invalid argument" << endl;
            usage(argv[0]);
            return 8;
        }
    }
    else if(argc > 2)
    {
        cout << "too many arguments" << endl;
        usage(argv[0]);
        return 7;
    }

    to_front("com.android.systemui");

#if DEBUG
    cout << "setting up sfdroid directory" << endl;
#endif
    mkdir(SFDROID_ROOT, 0770);
    unlink(AM_START_STILL_RUNNING_FILE);

    if(wayland_helper::init(windowmanager) != 0)
    {
        err = 1;
        goto quit;
    }

    if(sfconnection.init() != 0)
    {
        err = 2;
        goto quit;
    }

    if(windowmanager.init(sfconnection) != 0)
    {
        err = 7;
        goto quit;
    }

    if(sensorconnection.init() != 0)
    {
        err = 6;
        goto quit;
    }

    sfconnection.start_thread();
    sfconnection.gained_focus();
    sensorconnection.start_thread();

    if(!multiwindow)
    {
        windowmanager.handle_layer_name_event((char*)"com.android.systemui");
    }

    gettimeofday(&old_time, NULL);

    while(running)
    {
        wayland_helper::dispatch();

        sfdroid_events_mutex.lock();
        if(sfdroid_events.size() > 0)
        {
            for(vector<sfdroid_event>::size_type i = 0;i < sfdroid_events.size();i++)
            {
                switch(sfdroid_events[i].type)
                {
                    case LAST_WINDOW_CLOSED:
                        printf("last window closed\n");
                        sfdroid_events_mutex.unlock();
                        err = 0;
                        goto quit;
                        break;
                    case LAYER_NAME:
                        if(multiwindow) windowmanager.handle_layer_name_event(sfdroid_events[i].data.layer_name);
                        break;
                    case LAYER_CLOSE:
                        if(multiwindow) windowmanager.handle_layer_close_event(sfdroid_events[i].data.layer_name);
                        break;
                    case BUFFER:
                        if(!to_front_still_processing())
                        {
                            if(!windowmanager.handle_buffer_event(sfdroid_events[i].data.buffer.buffer, *sfdroid_events[i].data.buffer.info))
                            {
                                sfconnection.notify_buffer_done(1);
                                failed_frames++;
                                break;
                            }
                        }
                        frames++;
                        sfconnection.notify_buffer_done(0);
                        break;
                    case NO_BUFFER:
                        if(!to_front_still_processing())
                        {
                            if(!windowmanager.handle_no_buffer_event(sfdroid_events[i].data.buffer.buffer, *sfdroid_events[i].data.buffer.info))
                            {
                                failed_dummy_frames++;
                                sfconnection.notify_buffer_done(1);
                                break;
                            }
                        }
                        dummy_frames++;
                        sfconnection.notify_buffer_done(0);
                        break;
                }
            }

            sfdroid_events.clear();
        }
        sfdroid_events_mutex.unlock();

#if DEBUG
        cout << "waiting for event" << endl;
#endif
        if(sfconnection.have_focus())
        {
            sfconnection.wait_for_event(DUMMY_RENDER_TIMEOUT_MS);
            gettimeofday(&current_time, NULL);
            if(current_time.tv_sec > old_time.tv_sec)
            {
                cout << "frames: " << frames << endl;
                cout << "failed(ignored) frames: " << failed_frames << endl;
                cout << "dummy frames: " << dummy_frames << endl;
                cout << "failed(ignored) dummy frames: " << failed_dummy_frames << endl;
                cout << endl;
                old_time = current_time;
                frames = failed_frames = dummy_frames = failed_dummy_frames = 0;
            }
        }
        else
        {
            // TODO
            sfconnection.wait_for_event(DUMMY_RENDER_TIMEOUT_MS * 3);
        }
    }

quit:
    sensorconnection.stop_thread();
    sensorconnection.deinit();
    sfconnection.stop_thread();
    sfconnection.deinit();
    windowmanager.deinit();
    wayland_helper::deinit();
    rmdir(SFDROID_ROOT);
    return err;
}


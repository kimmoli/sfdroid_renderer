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
#include <algorithm>

#include "wayland_helper.h"
#include "windowmanager.h"
#include "utility.h"

using namespace std;

int find_slot(vector<int> &slot_to_fingerId, int fingerId)
{
    // find the slot
    vector<int>::iterator it = find(slot_to_fingerId.begin(), slot_to_fingerId.end(), fingerId);
    if(it != slot_to_fingerId.end())
    {
        return std::distance(slot_to_fingerId.begin(), it);
    }

    // find first free slot
    vector<int>::size_type i;
    for(i = 0;i < slot_to_fingerId.size();i++)
    {
        if(slot_to_fingerId[i] == -1)
        {
            slot_to_fingerId[i] = fingerId;
            return i;
        }
    }

    // no free slot found, add new one
    slot_to_fingerId.resize(slot_to_fingerId.size()+1);
    slot_to_fingerId[slot_to_fingerId.size()-1] = fingerId;

    return slot_to_fingerId.size()-1;
}

void erase_slot(vector<int> &slot_to_fingerId, int fingerId)
{
    vector<int>::iterator it = find(slot_to_fingerId.begin(), slot_to_fingerId.end(), fingerId);

    if(it != slot_to_fingerId.end())
    {
        vector<int>::size_type idx = distance(slot_to_fingerId.begin(), it);

        slot_to_fingerId[idx] = -1;

        // if we're at the end of the vector, erase unneeded elements
        if(idx == slot_to_fingerId.size()-1)
        {
            while(slot_to_fingerId[idx] == -1)
            {
                slot_to_fingerId.resize(idx);
                idx--;
            }
        }
    }
    else
    {
        cerr << "BUG: erase_slot" << endl;
    }
}

int windowmanager_t::init(sfconnection_t &sfc)
{
    sfconnection = &sfc;

    swipe_hack_dist_x = (SWIPE_HACK_PIXEL_PERCENT * wayland_helper::width) / 100;
    swipe_hack_dist_y = (SWIPE_HACK_PIXEL_PERCENT * wayland_helper::height) / 100;

#if DEBUG
    cout << "swipe hack dist (x,y): (" << swipe_hack_dist_x << "," << swipe_hack_dist_y << ")" << endl;
#endif

    return uinput.init(wayland_helper::width, wayland_helper::height);
}

void windowmanager_t::deinit()
{
    for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
    {
        stop_app(it->second->get_package().c_str());
        stop_app(it->second->get_package().c_str());
        it->second->deinit();
        delete it->second;
    }
}

void windowmanager_t::take_focus()
{
    for(map<string, renderer_t*>::iterator wit = windows.begin();wit != windows.end();wit++)
    {
        if(wit->second->is_active())
        {
            wit->second->lost_focus();
            taken_focus = wit->second;
        }
    }
}

void windowmanager_t::seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    windowmanager_t *windowmanager = (windowmanager_t*)data;

#if DEBUG
    cout << "seat handle caps: " << caps << endl;
#endif

    if((caps & WL_SEAT_CAPABILITY_TOUCH))
    {
        windowmanager->w_touch = wl_seat_get_touch(seat);
        wl_touch_add_listener(windowmanager->w_touch, &(windowmanager->w_touch_listener), data);
    }

    if((caps & WL_SEAT_CAPABILITY_KEYBOARD))
    {
        windowmanager->w_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(windowmanager->w_keyboard, &(windowmanager->w_keyboard_listener), data);
    }
}

void windowmanager_t::handle_layer_name_event(char *layer_name)
{
#if DEBUG
    cout << "handle layer name event " << layer_name << endl;
#endif

    if(is_blacklisted(layer_name))
    {
        // seems like a new app is starting... lets hope...
        // if a window that is toplevel here it will gain focus again @ gain focus again
        take_focus();
        if(last_layer.find("Android is starting") == std::string::npos) return;
    }

    last_layer = layer_name;

    wait_for_next_layer_name = false;

    string app = get_app_name(layer_name);

    map<string, renderer_t*>::iterator wit = windows.find(app);

    if(wit == windows.end() && last_layer.find("Android is starting") != std::string::npos)
    {
        app = "com.android.systemui";
    }

    if(wit != windows.end())
    {
        if(!wit->second->is_active())
        {
            take_focus();
            // close it to bring it to the front...
            if(taken_focus != wit->second)
            {
                wit->second->recreate();
                taken_focus = nullptr;
            }
            else
            {
                // gain focus again
                wit->second->gained_focus();
                taken_focus = nullptr;
            }
        }
    }
    else
    {
        take_focus();
        windows[app] = new renderer_t();
        windows[app]->init(*this);
        windows[app]->set_package(app);
        windows[app]->gained_focus();
        taken_focus = nullptr;
    }
}

void windowmanager_t::handle_layer_close_event(char *layer_name)
{
#if DEBUG
    cout << "handle layer close event " << layer_name << endl;
#endif

    if(is_blacklisted(layer_name))
    {
        take_focus();
        return;
    }

    string app = get_app_name(layer_name);

    // why does this happen?
    if(app == "com.android.systemui")
    {
        take_focus();
        return;
    }

    map<string, renderer_t*>::iterator wit = windows.find(app);

    if(wit != windows.end())
    {
        take_focus();
        wit = windows.find(app);
        wit->second->deinit();
        delete wit->second;
        windows.erase(wit);
        if(windows.size() == 0)
        {
            sfdroid_event event;
            event.type = LAST_WINDOW_CLOSED;
            sfdroid_events_mutex.lock();
            sfdroid_events.push_back(event);
            sfdroid_events_mutex.unlock();
        }
    }
}

bool windowmanager_t::handle_buffer_event(ANativeWindowBuffer *buffer, buffer_info_t &info)
{
#if DEBUG
    cout << "handle buffer event" << endl;
#endif

    for(map<string, renderer_t*>::iterator wit = windows.begin();wit != windows.end();wit++)
    {
        if(wit->second->is_active() && !wait_for_next_layer_name)
        {
            if(wit->second->render_buffer(buffer, info) != 0)
            {
                return false;
            }

            return true;
        }
    }

    return false;
}

bool windowmanager_t::handle_no_buffer_event(ANativeWindowBuffer *old_buffer, buffer_info_t &info)
{
#if DEBUG
    cout << "handle no buffer event" << endl;
#endif

    for(map<string, renderer_t*>::iterator wit = windows.begin();wit != windows.end();wit++)
    {
        if(wit->second->is_active())
        {
            wit->second->render_buffer(old_buffer, info);
            return true;
        }
    }

    return false;
}

void windowmanager_t::handle_close(struct wl_surface *surface)
{
#if DEBUG
    cout << "handle close" << endl;
#endif

    for(map<string, renderer_t*>::iterator wit = windows.begin();wit != windows.end();wit++)
    {
        if(wit->second->get_surface() == surface)
        {
            stop_app(wit->second->get_package().c_str());
            wit->second->deinit();
            delete wit->second;
            windows.erase(wit);
            if(windows.size() == 0)
            {
                sfdroid_event event;
                event.type = LAST_WINDOW_CLOSED;
                sfdroid_events_mutex.lock();
                sfdroid_events.push_back(event);
                sfdroid_events_mutex.unlock();
            }
            break;
        }
    }
}

void windowmanager_t::touch_handle_down(void *data, struct wl_touch *wl_touch, uint32_t serial, uint32_t time, struct wl_surface *surface, int32_t id, wl_fixed_t w_x, wl_fixed_t w_y)
{
#if DEBUG
    cout << "handle touch down event" << endl;
#endif

    windowmanager_t *windowmanager = (windowmanager_t*)data;
    int x, y;
    int touch_x = wl_fixed_to_int(w_x);
    int touch_y = wl_fixed_to_int(w_y);
    int slot;

    slot = find_slot(windowmanager->slot_to_fingerId, id);

    windowmanager->uinput.send_event(EV_ABS, ABS_MT_SLOT, slot);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, id);

    x = touch_x;

    if(touch_x <= windowmanager->swipe_hack_dist_x)
    {
#if DEBUG
        cout << "swipe hack x" << endl;
#endif
        x = 0;
    }
    if(touch_x >= wayland_helper::width - windowmanager->swipe_hack_dist_x)
    {
#if DEBUG
        cout << "swipe hack x" << endl;
#endif
        x = wayland_helper::width;
    }

    y = touch_y;

    if(touch_y <= windowmanager->swipe_hack_dist_y)
    {
#if DEBUG
        cout << "swipe hack y" << endl;
#endif
        y = 0;
    }
    if(touch_y >= wayland_helper::height - windowmanager->swipe_hack_dist_y)
    {
#if DEBUG
        cout << "swipe hack y" << endl;
#endif
        y = wayland_helper::height;
    }

    windowmanager->uinput.send_event(EV_ABS, ABS_MT_POSITION_X, x);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_POSITION_Y, y);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_PRESSURE, MAX_PRESSURE);
    windowmanager->uinput.send_event(EV_SYN, SYN_REPORT, 0);
}

void windowmanager_t::touch_handle_up(void *data, struct wl_touch *wl_touch, uint32_t serial, uint32_t time, int32_t id)
{
#if DEBUG
    cout << "handle touch up event" << endl;
#endif

    windowmanager_t *windowmanager = (windowmanager_t*)data;
    int slot;

    slot = find_slot(windowmanager->slot_to_fingerId, id);

    windowmanager->uinput.send_event(EV_ABS, ABS_MT_SLOT, slot);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
    windowmanager->uinput.send_event(EV_SYN, SYN_REPORT, 0);

    erase_slot(windowmanager->slot_to_fingerId, id);
}

void windowmanager_t::touch_handle_motion(void *data, struct wl_touch *wl_touch, uint32_t time, int32_t id, wl_fixed_t w_x, wl_fixed_t w_y)
{
#if DEBUG
    cout << "handle touch motion event" << endl;
#endif

    windowmanager_t *windowmanager = (windowmanager_t*)data;
    int touch_x = wl_fixed_to_int(w_x);
    int touch_y = wl_fixed_to_int(w_y);
    int slot;

    slot = find_slot(windowmanager->slot_to_fingerId, id);

    windowmanager->uinput.send_event(EV_ABS, ABS_MT_SLOT, slot);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, id);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_POSITION_X, touch_x);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_POSITION_Y, touch_y);
    windowmanager->uinput.send_event(EV_ABS, ABS_MT_PRESSURE, MAX_PRESSURE);
    windowmanager->uinput.send_event(EV_SYN, SYN_REPORT, 0);
}

void windowmanager_t::touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
#if DEBUG
    cout << "handle touch frame event" << endl;
#endif
}

void windowmanager_t::touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
#if DEBUG
    cout << "handle keyboard cancel event" << endl;
#endif
}

void windowmanager_t::keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
#if DEBUG
    cout << "handle keyboard keymap event" << endl;
#endif
}

void windowmanager_t::keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
#if DEBUG
    cout << "handle keyboard enter event" << endl;
#endif

    windowmanager_t *windowmanager = (windowmanager_t*)data;

    for(map<string, renderer_t*>::iterator wit = windowmanager->windows.begin();wit != windowmanager->windows.end();wit++)
    {
        if(wit->second->get_surface() == surface)
        {
            if(!wit->second->is_active())
            {
                windowmanager->take_focus();
                wakeup_android();
                to_front(wit->second->get_package().c_str());
                windowmanager->wait_for_next_layer_name = true;
                wit->second->gained_focus();
                windowmanager->taken_focus = nullptr;
            }
            break;
        }
    }

    windowmanager->sfconnection->gained_focus();
}

void windowmanager_t::keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface)
{
#if DEBUG
    cout << "handle keyboard leave event" << endl;
#endif

    windowmanager_t *windowmanager = (windowmanager_t*)data;

    for(map<string, renderer_t*>::iterator wit = windowmanager->windows.begin();wit != windowmanager->windows.end();wit++)
    {
        if(wit->second->get_surface() == surface)
        {
            if(wit->second->is_active())
            {
                wit->second->lost_focus();
                break;
            }
        }
    }

    windowmanager->sfconnection->lost_focus();
}

void windowmanager_t::keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
#if DEBUG
    cout << "handle keyboard key event" << endl;
#endif
}

void windowmanager_t::keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
#if DEBUG
    cout << "handle keyboard modifier event" << endl;
#endif
}
 
void windowmanager_t::keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
#if DEBUG
    cout << "handle keyboard repeat info event" << endl;
#endif
}


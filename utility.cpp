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

#include "utility.h"
#include "sfdroid_defs.h"
#include <android-version.h>

#include <fstream>
#include <string>
#include <unistd.h>
#include <cstring>

using namespace std;

// these should stay in the same window
bool is_blacklisted(string app)
{
    if(app.find("BootAnimation") == 0)
    {
        return false;
    }

    if(app.find(".") == std::string::npos)
    {
        return true;
    }

    if(app.find("android/com.android.internal.app.ResolverActivity") != std::string::npos)
    {
        return true;
    }

    if(app.find("com.android.phone") != std::string::npos)
    {
        return true;
    }

    return false;
}

string get_app_name(char *layer_name)
{
    std::string app = layer_name;
    std::string::size_type idx = app.find("SurfaceView ");

    if(app == "BootAnimation")
    {
        return "com.android.systemui";
    }

    if(app.find("Android is starting") != std::string::npos)
    {
        return "com.android.systemui";
    }

    if(idx != std::string::npos)
    {
        app = app.substr(idx + 12);
    }

    idx = app.find("Starting ");
    if(idx != std::string::npos)
    {
        app = app.substr(idx + 9);
    }

    if(app == "")
    {
        app = layer_name;
    }

    idx = app.find(" ");

    if(idx != std::string::npos)
    {
        app = app.substr(0, idx);
    }

    if(app == "com.android.phasebeam.PhaseBeamWallpaper" || app.find("com.cyanogenmod.trebuchet") != std::string::npos)
    {
        app = "com.android.systemui";
    }

    idx = app.find("/");
    if(idx != std::string::npos)
    {
        app = app.substr(0, idx);
    }

    return app;
}

void touch(const char *fname)
{
    fstream f;
    f.open(fname, ios::out);
}

void wakeup_android()
{
#if ANDROID_VERSION_MAJOR == 4 && ANDROID_VERSION_MINOR == 4
    system("/usr/bin/sfdroid_powerup &");
#else
    system("/usr/bin/sfdroid_powerup_cm12.1 &");
#endif
}

void to_front(const char *app)
{
    char buff[5120];
    touch(AM_START_STILL_RUNNING_FILE);
    if(strcmp(app, "com.android.systemui") != 0)
    {
        snprintf(buff, 5120, "(/usr/bin/am previous --user 0 -p %s; rm %s) &", app, AM_START_STILL_RUNNING_FILE);
    }
    else
    {
        snprintf(buff, 5120, "(/usr/bin/am previous --user 0 -p com.cyanogenmod.trebuchet; rm %s) &", AM_START_STILL_RUNNING_FILE);
    }
    system(buff);
}

bool to_front_still_processing()
{
    fstream f;
    f.open(AM_START_STILL_RUNNING_FILE, ios::in);
    if(f.is_open()) return true;
    return false;
}

void start_app(const char *appandactivity)
{
    char buff[5120];
    snprintf(buff, 5120, "/usr/bin/am start --user 0 -n %s &", appandactivity);
    system(buff);
}

void go_home()
{
    system("/usr/bin/am start --user 0 -c android.intent.category.HOME -a android.intent.action.MAIN &");
}

void stop_app(const char *appandactivity)
{
    char app[5120];
    char buff[5120];
    strncpy(app, appandactivity, 5120);
    char *slash = strstr(app, "/");
    if(slash != NULL) *slash = 0;
    snprintf(buff, 5120, "/usr/bin/am force-stop --user 0 %s &", app);
    system(buff);
}


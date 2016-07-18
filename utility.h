#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <string>

void touch(const char *fname);
void wakeup_android();

void go_home();
void to_front(const char *app);
bool to_front_still_processing();
void start_app(const char *appandactivity);
void stop_app(const char *appandactivity);
bool is_blacklisted(std::string app);
std::string get_app_name(char *layer_name);

#endif


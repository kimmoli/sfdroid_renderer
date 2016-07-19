#ifndef __SF_CONNECTION_H__
#define __SF_CONNECTION_H__

#include <thread>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <mutex>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <system/window.h>

#include "sfdroid_defs.h"

extern gralloc_module_t *gralloc_module;

class sfconnection_t {
    public:
        sfconnection_t() : current_status(0), fd_pass_socket(-1), fd_client(-1), running(false), current_buffer(nullptr), timeout_count(0), my_have_focus(true), notified(false) {}
        int init();
        void deinit();
        int wait_for_client();
        buffer_info_t *get_current_info();
        ANativeWindowBuffer *get_current_buffer();
        void wait_for_event(int timeout);
        void start_thread();
        void thread_loop();
        void stop_thread();
        void update_timeout();
        bool have_client();
        bool have_focus() { return my_have_focus; }
        void notify_buffer_done(int failed);

        void remove_buffers();

        void lost_focus();
        void gained_focus();

    private:
        int wait_for_buffer(int &timedout, bool &is_not_a_buffer);
        void send_status_and_cleanup();
        int current_status;
        bool thread_exited;

        int fd_pass_socket; // listen for surfaceflinger
        int fd_client; // the client (sharebuffer module)

        std::thread my_thread;
        std::atomic<bool> running;
        std::condition_variable buffer_cond;
        std::condition_variable back_cond;
        std::mutex notify_mutex, notify_back_mutex;

        buffer_info_t current_info;
        ANativeWindowBuffer *current_buffer;
        unsigned int timeout_count;

        bool my_have_focus;
        bool notified;

        std::vector<ANativeWindowBuffer*> buffers;
        std::vector<buffer_info_t> buffer_infos;
};

#endif


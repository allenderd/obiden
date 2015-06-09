#pragma once
#if 0
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>

#include "host.h"

using std::thread;
using std::mutex;
using std::condition_variable;
using std::unique_lock;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::system_clock;

namespace obiden {

class Timer {
public:


    enum WaitTime {
        ELECTION_RANDOM = -1,
        HEARTBEAT = 100,
        VICE_PRESIDENT = 50,
    };

    static WaitTime wait_time;
    static system_clock::duration vp_start_time;
    static system_clock::duration vp_elapsed_time;
    static mutex timer_mutex;
    static condition_variable timer_cv;

    static void Run();
    static void Reset();
    static void ChangeState(HostState host_state);
};

}
#endif

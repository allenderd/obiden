#include "timer.h"

namespace obiden {

Timer::WaitTime Timer::wait_time;
system_clock::duration Timer::vp_start_time;
system_clock::duration Timer::vp_elapsed_time;
mutex Timer::timer_mutex;
condition_variable Timer::timer_cv;

void Timer::Run() {
    wait_time = ELECTION_RANDOM;
    srand (time(NULL));
    int rand_timeout = rand() % 150 + 150;
    //auto rand_timeout = std::bind(std::uniform_int_distribution<int>(150, 300),
    //    std::default_random_engine(high_resolution_clock::now().time_since_epoch().count()));
    while (true) {
        unique_lock<mutex> lock(timer_mutex);
        auto wait_ms = wait_time == ELECTION_RANDOM ? milliseconds(rand_timeout) :
            milliseconds(wait_time);
        auto status = timer_cv.wait_for(lock, wait_ms);
        if (status) {
			// if (Host::CheckState() == HostState::FOLLOWER) {
			// 	Host::ChangeState(HostState::CANDIDATE);
			// }
            unique_lock<mutex> event_lock(Host::event_mutex);
            Host::event_cv.notify_one();
        }
    }
}

void Timer::Reset() {
    unique_lock<mutex> lock(timer_mutex);
    timer_cv.notify_one();
}
void Timer::ChangeState(HostState host_state) {
    unique_lock<mutex> lock(timer_mutex);
    if (host_state == HostState::PRESIDENT) {
        wait_time = WaitTime::HEARTBEAT;
    }
    else if (host_state == HostState::CANDIDATE || host_state == HostState::FOLLOWER) {
        wait_time = WaitTime::ELECTION_RANDOM;
    }
    else {
        wait_time = WaitTime::VICE_PRESIDENT;
    }
    timer_cv.notify_one();
}
}

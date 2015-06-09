#define _GLIBCXX_USE_NANOSLEEP
#include <thread>
#include <iostream>
#include <fstream>
#include <string>

#include "host.h"
#include "networking.h"
//#include "timer.h"

using std::thread;
using std::ifstream;
using std::string;

using namespace obiden;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: <config_file> <config_host_index>." << std::endl;
        return 1;
    }

    int self_index = std::stoi(argv[2]);

    vector<HostInfo> hostinfo_vector;
    HostInfo hostinfo;
    ifstream input(argv[1]);
    string ip;
    while (std::getline(input, ip)) {
        int port_start = ip.find(':');
        hostinfo.hostname = ip.substr(0, port_start);
        hostinfo.port = std::stoi(ip.substr(port_start + 1));
        hostinfo_vector.push_back(hostinfo);
    }


    auto client_host = hostinfo_vector.back();
    hostinfo_vector.pop_back();

    Network::Init(hostinfo_vector, client_host);
    Host::Init(hostinfo_vector.size(), self_index);

    auto listener_thread = thread(Network::CreateListener, hostinfo_vector[self_index].port);
    std::cout << "launched listener thread" << std::endl;
    //auto timer_thread = thread(Timer::Run);
    //std::cout << "launched timer thread" << std::endl;
    while (true) {

	// anytime the host state changes, the event_cv should be signaled
	unique_lock<mutex> lock(Host::event_mutex);
	//Host::event_cv.wait(lock);
        auto sleep_time = Host::sleep_time;
        lock.unlock();

        //std::cout << "before state choice released e4" << std::endl;

        sleep_for(sleep_time);

        switch (Host::host_state) {
        case HostState::PRESIDENT:
            std::cout << "president state" << std::endl;
            Host::PresidentState();
            break;
#ifndef RAFT_MODE
        case HostState::VICE_PRESIDENT:
            std::cout << "VP state" << std::endl;
            Host::VicePresidentState();
            break;
#endif
        case HostState::CANDIDATE:
        std::cout << "Candidate state" << std::endl;

            Host::CandidateState();
            break;
        case HostState::FOLLOWER:
        std::cout << "Follower State" << std::endl;

            Host::FollowerState();
            break;
        default:
            // be super sad, something went bad.
            break;
        }
    }

    return 0;
}

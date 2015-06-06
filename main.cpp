#include "host.h"
#include <thread>
#include <iostream>
#include <fstream>
#include <string>

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
    auto input = ifstream(argv[1]);
    string ip;
    while (std::getline(input, ip)) {
        int port_start = ip.find(':');
        hostinfo.hostname = ip.substr(0, port_start);
        hostinfo.port = std::stoi(ip.substr(port_start + 1));
        hostinfo_vector.push_back(hostinfo);
    }


    auto client_host = hostinfo_vector.back();
    hostinfo_vector.pop_back();

    Network network(hostinfo_vector, client_host);

    Host host(hostinfo_vector.size(), self_index, network);

    thread listener_thread(Network::CreateListener, &host, 
        hostinfo_vector[self_index].port);

    // create timer thread

    while (true) {
        switch (host.host_state) {
        case HostState::PRESIDENT:
            host.PresidentState();
            break;
        case HostState::VICE_PRESIDENT:
            host.VicePresidentState();
            break;
        case HostState::CANDIDATE:
            host.CandidateState();
            break;
        case HostState::FOLLOWER:
            host.FollowerState();
            break;
        default:
            // be super sad, something went bad.
            break;
        }

    }

    return 0;
}
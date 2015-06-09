#define _GLIBCXX_USE_NANOSLEEP
#include <thread>
#include <iostream>
#include <fstream>
#include <string>

#include "networking.h"
#include "packets.h"

using std::chrono::milliseconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;

int main(int argc, char* argv[]) {

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
    
    auto num_hosts = hostinfo.size();
    auto num_direct_hosts = num_hosts;
    int vp_index = -1;
    uint16_t vp_hosts_bits = 0;
    vector<int> others_indices;
    for (int i = 0; i < num_direct_hosts; ++i) {
        others_hosts.push_back(i);
    }
        
    if (argc == 3 && argv[2][0] == 'v' && argv[2][1]) {
        num_direct_hosts = num_hosts / 2;
        vp_index = 1;
        
        for (int i = num_direct_hosts; i < num_hosts; ++i) {
            uint16_t mask = 1 << i;
            vp_hosts_bits |= mask;
        }
    }    
    
    int data = 0;
    
    AppendEntriesPacket packet(1, 0, 1, 0, 0, vp_index, uint16_t vp_hosts_bits) : header(term, previous_log_index,
        previous_log_term, commit_index, president_index, vp_index, vp_hosts_bits)
    
    while (true) {
        sleep_for(milliseconds(300));
        Network::SendPackets(packet.ToNetworkOrder().ToBytes(), LARGE_PACKET_SIZE, others_hosts);
    }

    return 0;
}

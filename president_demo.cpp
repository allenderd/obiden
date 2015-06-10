#define _GLIBCXX_USE_NANOSLEEP
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <memory.h>
#include <netdb.h>

#include "packets.h"

using std::chrono::milliseconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;
using std::string;
using std::vector;
using std::ifstream;
using std::cout;

struct HostInfo {
    string hostname;
    int port;
};

vector<HostInfo> host_info_vector;
HostInfo host_info;

void SendPacket(uint8_t *payload, int payload_size, HostInfo host_info)
{
    int sk = 0;
    struct sockaddr_in remote;
    struct hostent *hp;

    auto packet = new char[payload_size];
    memcpy(packet, payload, payload_size);

    if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket call");
        exit(-1);
    }

    remote.sin_family = AF_INET;
    hp = gethostbyname(host_info.hostname.c_str());
    memcpy(&remote.sin_addr, hp->h_addr, hp->h_length);
    remote.sin_port = htons(host_info.port);

    std::cout << "sending packet; size: " << *((uint16_t*)packet) << " opcode: " <<  *((uint16_t*)(packet+2)) << std::endl;

    sendto(sk, packet, payload_size, 0, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));

    close(sk);

    delete[] packet;
}

void SendPackets(uint8_t *payload, int payload_size, const vector<int>& indices)
{
    for (auto index = indices.begin(); index != indices.end(); index++)
    {
        SendPacket(payload, payload_size, host_info_vector[*index]);
    }
}

int main(int argc, char* argv[]) {
   
    ifstream input(argv[1]);
    string ip;
    while (std::getline(input, ip)) {
        int port_start = ip.find(':');
        host_info.hostname = ip.substr(0, port_start);
        host_info.port = std::stoi(ip.substr(port_start + 1));
        host_info_vector.push_back(host_info);
    }
    
    auto num_hosts = host_info_vector.size();
    auto num_direct_hosts = num_hosts - 1;
    int vp_index = -1;
    uint16_t vp_hosts_bits = 0;
    vector<int> others_indices;
        
    if (argc == 3 && argv[2][0] == 'v' && argv[2][1]) {
        num_direct_hosts = num_hosts / 2;
        vp_index = 1;
        
        for (int i = num_direct_hosts + 1; i < num_hosts; ++i) {
            uint16_t mask = 1 << i;
            vp_hosts_bits |= mask;
        }
    }    

    for (int i = 1; i <= num_direct_hosts; ++i) {
        others_indices.push_back(i);
    }
    
    
    
    int data = 0;
    
    AppendEntriesPacket packet(1, 0, 1, 0, 0, vp_index, vp_hosts_bits);
    
    while (true) {
        sleep_for(milliseconds(1000));
        SendPackets(packet.ToBytes(), LARGE_PACKET_SIZE, others_indices);
        ++packet.header.previous_log_index;
        *reinterpret_cast<uint16_t*>(&packet.data) = data;
        ++data;
    }

    return 0;
}

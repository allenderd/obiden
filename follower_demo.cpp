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
using std::thread;

struct HostInfo {
    string hostname;
    int port;
};

vector<HostInfo> host_info_vector;
HostInfo host_info;
int self_index;

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
        cout << "Sending packet[" << *index << "]\n";
        cout << "HostInfo.size() = " << host_info_vector.size() << "\n";
        cout << "HostInfo[" << *index << "] = " << host_info_vector[*index].hostname << "\n";
        SendPacket(payload, payload_size, host_info_vector[*index]);
    }
}

void ProcessPacket(uint8_t* raw_packet) {
    auto packet = reinterpret_cast<AppendEntriesPacket*>(raw_packet);
    auto size = packet->header.size;
    auto opcode = packet->header.opcode;
    auto vp_index = packet->header.vp_index;
    auto data = *reinterpret_cast<uint16_t*>(packet->data);
    cout << "Got a packet!\n\t Size is " << size << "\n\tOpcode is " << opcode <<
        "\n\tData is " << data << "\n\tVP index is " << int(vp_index) << "\n";
    if (vp_index == self_index) {
        cout << "I am VP!" << "\n\n";
        auto hosts_bits = packet->header.vp_hosts_bits;
        cout << "hosts_bits = " << hosts_bits << "\n";
        vector<int> hosts_indices;
        int index = 0;
        for (int i = 0; i < 16; ++i) {
            uint16_t mask = 1 << i;
            if (hosts_bits & mask) {
                hosts_indices.push_back(i);
                cout << "hosts_indices[" << index << "] = " << i << "\n";
                ++index;
            }
        }
        
        SendPackets(raw_packet, LARGE_PACKET_SIZE, hosts_indices);
        
    } else {
        cout << "\n";
    }
    
    
}

void CreateListener(int portnum)
{
    int sk = 0;
    struct sockaddr_in local;
    socklen_t len = sizeof(local);

    // Create listener socket and process packets here
    if((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket Call");
        exit(-1);
    }
    //set up the socket
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(portnum);

    //bind the name (address) to a port
    if(bind(sk,(struct sockaddr*)&local, sizeof(local)) < 0){
        perror("bind call");
        exit(-1);
    }

    int messageLength = 0;
    struct sockaddr_in remote;
    socklen_t rlen = sizeof(remote);

    if(getsockname(sk,(struct sockaddr*)&local,&len) < 0){
        perror("getsockname call");
        exit(-1);
    }
    printf("socket has port %d \n", ntohs(local.sin_port));

    // Wait for connection
    while (true) {
        // Wait for packets, and parse them as they come in
        // Can be 24 bytes or 1024, messageLength will be the determinant of what packet type it is
        auto packet = new uint8_t[LARGE_PACKET_SIZE];
        messageLength = recvfrom(sk, reinterpret_cast<char*>(packet), LARGE_PACKET_SIZE, 0,
            (struct sockaddr*) &remote, &rlen);
        std::cout << "mesglen: " << messageLength << "\nopcode: " << *((uint16_t*)(packet+2)) << '\n';
        auto dispatch_thread = thread(ProcessPacket, packet);
        dispatch_thread.detach();
    }

}

int main(int argc, char* argv[]) {
   
    if (argc != 3) {
        std::cerr << "Usage: <config_file> <config_host_index>." << std::endl;
        return 1;
    }

    self_index = std::stoi(argv[2]);

    ifstream input(argv[1]);
    string ip;
    while (std::getline(input, ip)) {
        int port_start = ip.find(':');
        host_info.hostname = ip.substr(0, port_start);
        host_info.port = std::stoi(ip.substr(port_start + 1));
        host_info_vector.push_back(host_info);
    }
   
    CreateListener(host_info_vector[self_index].port);

    return 0;
}

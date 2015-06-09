#define _GLIBCXX_USE_NANOSLEEP
#include "networking.h"
#include <iostream>
#include <thread>
#include "host.h"
#include <sys/socket.h>
#include <memory.h>
#include <netdb.h>

using std::thread;

using std::cout;

namespace obiden {

/*
 * SetupNetwork()
 * Opens the networking configuration file and sets up the multicast socket communication between
 * the nodes in the cluster.
 *
 * There will be a fixed number of hosts that will be part of the cluster, and the file layout will
 * be as such:
 *
 * <ip_address_host_1> <port_1>
 * [...]
 * <ip_address_host_n> <port_n>
 *
 */


 vector<HostInfo> Network::host_info_vector;
 HostInfo Network::client_info;

void Network::CreateListener(int portnum)
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
        std::cout << "mesglen: " << messageLength << "\npayload: " << packet << '\n';
        auto dispatch_thread = thread(Host::RoutePacket, packet);
    }

}

void Network::SendPackets(uint8_t *payload, int payload_size, const vector<int>& indices, bool to_client)
{
    for (auto index = indices.begin(); index != indices.end(); index++)
    {
        //auto host_thread = thread(SendPacketInThread, payload, payload_size, host_info_vector[*index]);
        SendPacketInThread(payload, payload_size, host_info_vector[*index]);
    }
    if (to_client) {
        //auto client_thread = thread(SendPacketInThread, payload, payload_size, client_info);
        SendPacketInThread(payload, payload_size, client_info);
    }

}

void Network::SendPacketInThread(uint8_t *payload, int payload_size, HostInfo host_info)
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

    sendto(sk, packet, payload_size, 0, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));

    close(sk);

    delete[] packet;
}

}

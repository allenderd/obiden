#ifdef WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <ctime>
#include <sys/socket.h>
#include <memory.h>
#include <netdb.h>

#include "networking.h"
#include "packets.h"

//using namespace std;
using namespace obiden;

// CLIENT ONLY
int presidentIndex = 0;
vector<HostInfo> host_info_vector;
uint32_t data = 1;

void SendPacketInThread(uint8_t *payload, int payload_size)
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
	hp = gethostbyname(host_info_vector[presidentIndex].hostname.c_str());
	memcpy(&remote.sin_addr, hp->h_addr, hp->h_length);
	remote.sin_port = htons(host_info_vector[presidentIndex].port);

	sendto(sk, packet, payload_size, 0, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));

        close(sk);

	delete[] packet;
}

void createListener(int portnum, vector<HostInfo> host_info_in)
{
    int sk = 0;
    struct sockaddr_in local;
    socklen_t len = sizeof(local);

	host_info_vector = host_info_in;

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

	uint8_t packet[SMALL_PACKET_SIZE];

    // Wait for connection
    while (true) {
        // Wait for packets, and parse them as they come in
        // Can be 24 bytes or 1024, messageLength will be the determinant of what packet type it is

        messageLength = recvfrom(sk, reinterpret_cast<char*>(packet), SMALL_PACKET_SIZE, 0,
            (struct sockaddr*) &remote, &rlen);
        std::cout << "mesglen: " << messageLength << "\npayload: " << packet << '\n';

		auto opcode = ntohl(*reinterpret_cast<uint16_t*>(packet + 2));

        if (opcode == Opcode::EMPTY_APPEND_ENTRIES) {
            presidentIndex = ntohl(reinterpret_cast<AppendEntriesPacket*>(packet)->header.president_index);
		}
		else if (opcode == Opcode::COMMIT_TO_CLIENT) {
			++data;
			time_t timestamp;
			struct tm* tm_info = localtime(&timestamp);

			ClientDataPacket data_packet(data, uint32_t(timestamp));

			char buffer[64];
            strftime(buffer, 25, "%H:%M:%S", tm_info);
            std::cout << "time: " << buffer << " go back to listen\n";
			SendPacketInThread(data_packet.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE);
        }

    }
}






// Send an initial message and then wait to hear back from the leader.
    // Why send initial message?
// Then send another message just as soon as you hear back.

// Rather than make the hosts keep track of performance the client can just record the timestamp of each commit message received from the president.
// Dump to standard out.

int main(int argc, char* argv[])
{
    vector<HostInfo> hostinfo_vector;
    HostInfo hostinfo;
    std::ifstream input(argv[1]);
    string ip;
    while (std::getline(input, ip)) {
        int port_start = ip.find(':');
        hostinfo.hostname = ip.substr(0, port_start);
        hostinfo.port = std::stoi(ip.substr(port_start + 1));
        hostinfo_vector.push_back(hostinfo);
    }

    auto client_host = hostinfo_vector.back();
    hostinfo_vector.pop_back();

    auto listener_thread = std::thread(createListener, hostinfo_vector[hostinfo_vector.size() - 1].port, hostinfo_vector);

    return 0;
}

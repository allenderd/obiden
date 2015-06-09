#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <iostream>

enum class HostState {
	FOLLOWER,
	CANDIDATE,
	VICE_PRESIDENT,
	PRESIDENT
};

#include "packets.h"
#include "networking.h"
#include "timer.h"

using std::mutex;
using std::condition_variable;
using std::vector;
using std::unique_lock;

namespace obiden {

struct LogEntry {
    uint32_t term;
    uint32_t timestamp;
    uint32_t data;
    LogEntry(uint32_t term = 0, uint32_t timestamp = 0, uint32_t data = 0): term(term),
        timestamp(timestamp), data(data) { }
    LogEntry& ToNetworkOrder() {
        term = htonl(term);
        timestamp = htonl(timestamp);
        data = htonl(data);
        return *this;
    }
    LogEntry& ToHostOrder() {
        term = ntohl(term);
        timestamp = ntohl(timestamp);
        data = ntohl(data);
        return *this;
    }
};

typedef std::vector<LogEntry> Log;

class Host {
    // persistent
    static uint32_t term;
    static uint8_t voted_for;
    static Log log;
    // volatile

    static uint32_t commit_index;
    static uint32_t last_log_index;
    static uint32_t self_index;
    static uint32_t president_index;
    static uint32_t vice_president_index;

    static uint32_t num_hosts;
    static uint32_t* hosts_next_index;
    static uint32_t* hosts_match_index;
    static int votes_received;

    static uint32_t vp_hosts_max_term;
    static uint16_t vp_hosts_bits;
    static uint16_t vp_hosts_success_bits;
    static uint16_t vp_hosts_responded_bits;
    static uint16_t vp_hosts_is_empty_bits;

    static vector<int> others_indices;
	static bool append_entry_request_sent;

	static uint32_t vp_max_log_index;
	static vector<uint32_t> vp_hosts_log_index_vector;
	static vector<uint32_t> vp_hosts_term_vector;
	static vector<bool> vp_hosts_success_vector;
	static vector<bool> vp_hosts_responded_vector;
	static vector<bool> vp_hosts_isempty_vector;

public:

    static HostState host_state;
    static mutex event_mutex;
    static condition_variable event_cv;

    static void Init(int num_hosts, int self_index) {
        Host::num_hosts = num_hosts;
        Host::self_index = self_index;

        others_indices.reserve(num_hosts - 1);
        for (int i = 0; i < num_hosts; ++i) {
            if (i != self_index) {
                others_indices.push_back(i);
            }
        }

		hosts_next_index = new uint32_t[num_hosts];
		hosts_match_index = new uint32_t[num_hosts];

		vp_hosts_log_index_vector.resize(num_hosts);


    }

    static void ChangeState(HostState host_state) {
		std::cout << "ChangeState getting lock\n";
        unique_lock<mutex> lock(event_mutex);
        Host::host_state = host_state;
        event_cv.notify_one();
		std::cout << "CHangeState releasing lock\n";
    }

    static HostState CheckState() {
        unique_lock<mutex> lock(event_mutex);
        return host_state;
    }

    static void PresidentState();
    static void VicePresidentState();
    static void CandidateState();
    static void FollowerState();

    static uint16_t ToUint16(uint8_t* data) {
        return (data[0] << 8) | data[1];
    }
    static uint32_t ToUint32(uint8_t* data) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }
    static void ToBytes(uint16_t value, uint8_t* data) {
        data[0] = static_cast<uint8_t>(value >> 8);
        data[1] = static_cast<uint8_t>(value & 0xFF);
    }
    static void ToBytes(uint32_t value, uint8_t* data) {
        data[0] = static_cast<uint8_t>(value >> 24);
        data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        data[3] = static_cast<uint8_t>((value)& 0xFF);
    }

	static void HandleClientData(uint8_t* raw_packet);
    static void HandleRequestVote(uint8_t* raw_packet);
    static void HandleRequestVoteResponse(uint8_t* raw_packet);
    static void HandleAppendEntries(uint8_t* raw_packet, bool is_empty);
    static void HandleAppendEntriesResponse(uint8_t* raw_packet, bool is_empty);
    static void VpHandleAppendEntriesResponse(uint32_t follower_term, bool follower_success,
		uint32_t follower_index, bool follower_is_empty, uint32_t follower_log_index);
    static void PresidentHandleAppendEntriesResponse(bool follower_success, uint32_t follower_index,
		bool is_empty, uint32_t log_entry);
    static void HandleRequestAppendEntries(uint8_t* raw_packet);
    static void HandleVpCombinedResponse(uint8_t* raw_packet);
    static void RoutePacket(uint8_t* packet);
};

}

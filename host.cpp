#include <cstdint>
#include <vector>
#include <map>
#include <memory.h>
#include <iostream>
#include <memory>
#include <random>
#include <chrono>
#ifdef WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "host.h"
#include "networking.h"
#include "timer.h"

using std::map;

namespace obiden {

uint32_t Host::term = 0;
uint8_t Host::voted_for = -1;
uint32_t Host::commit_index = 0;
uint32_t Host::self_index = 0;
uint32_t Host::president_index = -1;
uint32_t Host::vice_president_index = -1;
uint32_t Host::num_hosts = 0;
uint32_t* Host::hosts_next_index = NULL;
uint32_t* Host::hosts_match_index = NULL;
int Host::votes_received = 0;
uint32_t Host::vp_hosts_max_term = 0;
uint16_t Host::vp_hosts_bits = 0;
uint16_t Host::vp_hosts_success_bits = 0;
uint16_t Host::vp_hosts_responded_bits = 0;
uint16_t Host::vp_hosts_is_empty_bits = 0;
HostState Host::host_state = HostState::FOLLOWER;
mutex Host::event_mutex;
//condition_variable Host::event_cv;
vector<int> Host::others_indices;
vector<uint32_t> Host::vp_hosts_log_index_vector;
uint32_t Host::vp_max_log_index;
vector<bool> Host::vp_hosts_isempty_vector;
vector<bool> Host::vp_hosts_success_vector;
vector<bool> Host::vp_hosts_responded_vector;
bool Host::append_entry_request_sent;
system_clock::duration Host::sleep_time;


Log Host::log;

void Host::HandleClientData(uint8_t* raw_packet) {
	auto packet = reinterpret_cast<ClientDataPacket*>(raw_packet);
	uint32_t data = ntohl(packet->data);
	uint32_t timestamp = ntohl(packet->timestamp);
        TIME_PRINT("HandleClientData lock\n");
        unique_lock<mutex> event_lock(event_mutex);
	log.push_back(LogEntry(term, timestamp, data));
	TIME_PRINT("HandleClientData unlock\n");
}

void Host::HandleRequestVote(uint8_t* raw_packet) {
    //Timer::Reset();
    auto packet = reinterpret_cast<RequestVotePacket*>(raw_packet);
    uint32_t sender_term = ntohl(packet->term);
    uint8_t sender_index = static_cast<uint8_t>(ntohl(packet->candidate_index) & 0xFF);
    uint32_t sender_log_index = ntohl(packet->last_log_index);
    uint32_t sender_log_term = ntohl(packet->last_log_term);

    if (sender_term >= term) {

        TIME_PRINT("HandleRequestVote lock");
        unique_lock<mutex> lock(event_mutex);
        auto last_log_index = log.size() - 1;
        TIME_PRINT("HandleRequestVote unlock");
        lock.unlock();

        uint32_t vote = (voted_for == -1 || voted_for == sender_index) &&
            sender_log_term >= term && sender_log_index >= last_log_index;
        RequestVoteResponsePacket response(term, vote);
        Network::SendPacket(response.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, sender_index);
        term = sender_term;
        if (sender_term > term) {
            ChangeState(HostState::FOLLOWER);
        }
    }
    else {
        RequestVoteResponsePacket response(term, 0);
        Network::SendPacket(response.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, sender_index);
    }
}

void Host::HandleRequestVoteResponse(uint8_t* raw_packet) {
    auto packet = reinterpret_cast<RequestVoteResponsePacket*>(raw_packet);
    uint32_t sender_term = ntohl(packet->term);
    uint32_t sender_is_vote_granted = ntohl(packet->is_vote_granted);
    if (sender_term > term) {
        term = sender_term;
        ChangeState(HostState::FOLLOWER);
        return;
    }
    if (sender_is_vote_granted) {
        ++votes_received;
    }
    if (votes_received > num_hosts / 2) {
        ChangeState(HostState::PRESIDENT);

        TIME_PRINT("HandleRequestVoteResponse lock");
        unique_lock<mutex> lock(event_mutex);
        auto last_log_index = log.size() - 1;
        auto previous_term = log[last_log_index].term;
        TIME_PRINT("HandleRequestVoteResponse unlock");
        lock.unlock();

        for (int i = 0; i < num_hosts; ++i) {
            hosts_next_index[i] = last_log_index + 1;
            hosts_match_index[i] = 0;
        }
        EmptyAppendEntriesPacket packet(term, last_log_index, previous_term,
            commit_index, self_index, -1, 0);
        Network::SendPackets(packet.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, others_indices, true);
    }
}

void Host::HandleAppendEntries(uint8_t* raw_packet, bool is_empty) {
    ChangeState(HostState::FOLLOWER);
    auto packet = reinterpret_cast<AppendEntriesPacket*>(raw_packet);
    uint8_t  sender_vp_index = packet->header.vp_index;
    static uint8_t original_packet[LARGE_PACKET_SIZE];
    if (sender_vp_index == self_index) {
        memcpy(original_packet, raw_packet, is_empty ? SMALL_PACKET_SIZE : LARGE_PACKET_SIZE);
    }

    uint32_t sender_term = ntohl(packet->header.term);
    uint32_t sender_previous_log_index = ntohl(packet->header.previous_log_index);
    uint32_t sender_log_index = sender_previous_log_index + 1;
    uint32_t sender_previous_log_term = ntohl(packet->header.previous_log_term);
    uint32_t sender_commit_index = ntohl(packet->header.previous_log_term);
    uint8_t  sender_president_index = packet->header.president_index;
    // vice president index above
    uint16_t sender_vp_hosts_bits = ntohs(packet->header.vp_hosts_bits);

    TIME_PRINT("HandleAppendEntries lock (1)");
    unique_lock<mutex> event_lock(event_mutex);
    auto previous_log_term = log[sender_previous_log_index].term;
    auto log_term = log[sender_log_index].term;
    auto log_size = log.size();
    TIME_PRINT("HandleAppendEntries unlock (1)");
    event_lock.unlock();

    if (sender_term < term ||
        previous_log_term != sender_previous_log_term) {
        AppendEntriesResponsePacket response(term, 0, self_index, 0);
        Network::SendPacket(response.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, sender_president_index);
        return;
    }

    if (!is_empty) {
        auto log_entry = reinterpret_cast<LogEntry*>(packet->data)->ToHostOrder();
        
        TIME_PRINT("HandleAppendEntries lock (2)");
        unique_lock<mutex> event_lock(event_mutex);
        if (log_size > sender_log_index && log_term != log_entry.term) {
            log.resize(sender_log_index);
        }
        log.push_back(log_entry);
        TIME_PRINT("HandleAppendEntries unlock (2)");
        event_lock.unlock();

        if (sender_commit_index > commit_index) {
            auto last = log.size() - 1;
            commit_index = sender_commit_index > last ? last : sender_commit_index;
        }
    }

    term = sender_term;

    AppendEntriesResponsePacket response(term, 1, self_index, sender_log_index);
    Network::SendPacket(response.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, sender_president_index);

#ifndef RAFT_MODE

    if (vice_president_index == self_index) {
        ChangeState(HostState::VICE_PRESIDENT);
        vp_hosts_bits = sender_vp_hosts_bits;
        vp_hosts_responded_bits = 0;
        vp_hosts_success_bits = 0;
        vp_hosts_is_empty_bits = 0;
        vp_hosts_max_term = 0;
        std::vector<int> host_indices;
        for (int i = 0; i < sizeof(sender_vp_hosts_bits) * 8; ++i) {
            uint16_t mask = 1 << i;
            if (sender_vp_hosts_bits & mask) {
                host_indices.push_back(i);
            }
        }
        Network::SendPackets(original_packet, SMALL_PACKET_SIZE, host_indices);
    }

#endif
}

void Host::HandleAppendEntriesResponse(uint8_t* raw_packet, bool is_empty) {
    auto packet = reinterpret_cast<AppendEntriesResponsePacket*>(raw_packet);
    uint32_t sender_term = ntohl(packet->term);
    uint32_t sender_success = ntohl(packet->success);
    uint32_t sender_index = ntohl(packet->sender_index);
    uint32_t sender_log_index = ntohl(packet->log_index);

    if (CheckState() == HostState::FOLLOWER) {
        return;
    }

#ifndef RAFT_MODE
    if (CheckState() == HostState::VICE_PRESIDENT) {
        VpHandleAppendEntriesResponse(sender_term, sender_success, sender_index, is_empty, sender_log_index);
        return;
    }
#endif

    if (CheckState() == HostState::PRESIDENT) {
        if (term < sender_term) {
            ChangeState(HostState::FOLLOWER);
            return;
        }
        else {
            PresidentHandleAppendEntriesResponse(sender_success, sender_index, is_empty,
                sender_log_index);
        }
    }
}

#ifndef RAFT_MODE
void Host::VpHandleAppendEntriesResponse(uint32_t follower_term, bool follower_success,
    uint32_t follower_index, bool follower_is_empty, uint32_t follower_log_index) {

    if (follower_term > vp_hosts_max_term) {
        vp_hosts_max_term = follower_term;
    }
    if (follower_log_index > vp_max_log_index) {
        vp_max_log_index = follower_log_index;
    }
    vp_hosts_isempty_vector[follower_index] = follower_is_empty;
    vp_hosts_log_index_vector[follower_index] = follower_log_index;
    vp_hosts_success_vector[follower_index] = follower_success;
    vp_hosts_responded_vector[follower_index] = true;
    vp_hosts_responded_bits |= 1 << follower_index;
    if (vp_hosts_bits == vp_hosts_responded_bits) {
	VicePresidentState(); // this will send the combined response and switch to follower state
    }

}
#endif

void Host::PresidentHandleAppendEntriesResponse(bool follower_success, uint32_t follower_index,
    bool is_empty, uint32_t log_entry) {
    if (follower_success) {
        if (!is_empty) {
            hosts_next_index[follower_index] = log_entry;
            hosts_match_index[follower_index] = log_entry;
        }
    }
    else {
        --hosts_next_index[follower_index];
        // signal condition variable

    }

    // move up commit index, go backwards so that once you hit the new index you can stop
	auto old_commit_index = commit_index;
    for (auto index = log.size() - 1; index > commit_index; --index) {
        if (log[index].term != term) {
            break;
        }
        int num_hosts_with_entry = 0;
        for (int i = 0; i < num_hosts; ++i) {
            if (hosts_match_index[i] >= index) {
                ++num_hosts_with_entry;
            }
        }
        if (num_hosts_with_entry > num_hosts / 2) {
            commit_index = index;
            break;
        }
    }
    if (old_commit_index != commit_index) {
        CommitToClientPacket packet(commit_index);
        Network::SendPackets(packet.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, vector<int>(), true);
    }
}

#ifndef RAFT_MODE
void Host::HandleRequestAppendEntries(uint8_t* raw_packet) {

    if (CheckState() == HostState::PRESIDENT) {
        vice_president_index = -1;
        auto packet = reinterpret_cast<RequestAppendEntriesPacket*>(raw_packet);
        uint32_t sender_index = ntohl(packet->sender_index);

        TIME_PRINT("HandleRequestVoteResponse lock");
        unique_lock<mutex> lock(event_mutex);
        auto last_log_index = log.size() - 1;
        auto previous_term = log[last_log_index].term;
        TIME_PRINT("HandleRequestVoteResponse unlock");
        lock.unlock();

        EmptyAppendEntriesPacket response(term, last_log_index, previous_term,
            commit_index, self_index, -1, 0);

        Network::SendPacket(response.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, sender_index);
    }
}

void Host::HandleVpCombinedResponse(uint8_t* raw_packet) {
    auto packet = reinterpret_cast<VpCombinedResponsePacket*>(raw_packet);
    uint16_t sender_vp_hosts_bits = ntohs(packet->vp_hosts_bits);
    uint16_t sender_vp_hosts_responded_bits = ntohs(packet->vp_hosts_responded_bits);
    uint16_t sender_vp_hosts_success_bits = ntohs(packet->vp_hosts_success_bits);
    uint16_t sender_vp_hosts_is_empty_bits = ntohs(packet->vp_hosts_is_empty_bits);
    uint32_t sender_max_term = ntohl(packet->max_term);
    uint32_t sender_max_log_index = ntohl(packet->max_log_index);

    if (term < sender_max_term) {
        term = sender_max_term;
        ChangeState(HostState::FOLLOWER);
        return;
    }
    else {
        for (int i = 0; i < sizeof(uint16_t); ++i) {
            uint16_t mask = 1 << i;
            if (sender_vp_hosts_responded_bits & mask) {
                bool success = (sender_vp_hosts_success_bits & mask) != 0;
                bool is_empty = (sender_vp_hosts_is_empty_bits & mask) != 0;
                PresidentHandleAppendEntriesResponse(success, i, is_empty, sender_max_log_index);
            }
        }
    }
}
#endif

void Host::RoutePacket(uint8_t* packet) {
    auto opcode = ToUint16(packet + 2);
    switch (opcode) {
	case CLIENT_DATA:
		HandleClientData(packet);
		break;
    case REQUEST_VOTE:
        HandleRequestVote(packet);
        break;
    case REQUEST_VOTE_RESPONSE:
        HandleRequestVoteResponse(packet);
        break;
    case APPEND_ENTRIES:
        HandleAppendEntries(packet, false);
        break;
    case EMPTY_APPEND_ENTRIES:
        HandleAppendEntries(packet, true);
        break;
    case APPEND_ENTRIES_RESPONSE:
        HandleAppendEntriesResponse(packet, false);
        break;
    case EMPTY_APPEND_ENTRIES_RESPONSE:
        HandleAppendEntriesResponse(packet, true);
        break;
#ifndef RAFT_MODE
    case REQUEST_APPEND_ENTRIES:
        HandleRequestAppendEntries(packet);
        break;
    case VP_COMBINED_RESPONSE:
        HandleVpCombinedResponse(packet);
        break;
#endif
    default:
        // ignore packet
        break;
    }
    delete[] packet;
}

void Host::PresidentState() {
    // Figure out which hosts are out of date and group them by which index they are at.
    // Take the largest group and if it has at least three members, choose a member and make it the
    // vice president, send an append entries with his index as vp_index. Then send to all the other
    // groups one at a time.

    auto log_size = log.size();
    bool need_to_send_heartbeat = true;
    //while (CheckState() == HostState::PRESIDENT) {
    map<int, vector<int>> index_map;
    size_t max_group = 0;
    //int max_next_index = -1;
    for (int i = 0; i < num_hosts; ++i) {
        int host_next_index_i = -1;
        if (hosts_next_index[i] < log_size) {
            host_next_index_i = hosts_next_index[i];
        }
        if (index_map.count(hosts_next_index[i]) == 0) {
            index_map[hosts_next_index[i]] = vector<int>{i};
        }
        else {
            index_map[hosts_next_index[i]].push_back(i);
        }
        if (index_map[hosts_next_index[i]].size() > max_group) {
            max_group = index_map[hosts_next_index[i]].size();
            //max_next_index = hosts_next_index[i];
        }
    }


#ifdef RAFT_MODE
    bool found_vp = true;
#else
    bool found_vp = max_group < 3; // if group less than three don't find a vp
#endif
    for (auto group = index_map.begin(); group != index_map.end(); ++group) {
        int vp_index = -1;
        uint16_t vp_host_bits = 0;
        if (!found_vp && group->second.size() == max_group) {
            vp_index = group->second[0];
            for (auto i = group->second.begin(); i != group->second.end(); i++) {
                vp_host_bits |= 1 << *i;
            }
        }
        if (group->first == -1) {
            EmptyAppendEntriesPacket packet(term, log_size - 1, log[log_size - 1].term,
                commit_index, self_index, -1, 0);
            Network::SendPackets(packet.ToNetworkOrder().ToBytes(), LARGE_PACKET_SIZE, group->second, false);
        } else {
            auto packet = AppendEntriesPacket(term, group->first, log[group->first].term,
                commit_index, self_index, vp_index, vp_host_bits);
            Network::SendPackets(packet.ToNetworkOrder().ToBytes(), LARGE_PACKET_SIZE, group->second, false);
        }
    }
}

void Host::CandidateState() {
	std::cout << "in candidate state" << std::endl;
    ++term;
    voted_for = self_index;
    votes_received = 1;
    uint32_t log_term = -1;
    uint32_t last_log_index = -1;
    TIME_PRINT("CandidateState lock");
    std::unique_lock<mutex> lock(event_mutex);
    if (log.size() > 0) {
        last_log_index = log.size() - 1;
        log_term = log[last_log_index].term;
    }
    TIME_PRINT("CandidateState unlock");
    lock.unlock();

    RequestVotePacket request_vote(term, self_index, last_log_index, log_term);
	std::cout << "before request vote" << std::endl;
	auto packet_bytes = request_vote.ToNetworkOrder().ToBytes();
    Network::SendPackets(packet_bytes, SMALL_PACKET_SIZE, others_indices, false);
    std::cout << "after request vote" << std::endl;

}

void Host::FollowerState() {
    std::cout << "In Follower State" << std::endl;
    if (president_index == -1 || append_entry_request_sent) {	
        ChangeState(HostState::CANDIDATE);
    }
    else {
        std::cout << "(president_index = " << president_index << ")" << std::endl;
        append_entry_request_sent = true;
        RequestAppendEntriesPacket packet(self_index);
        Network::SendPacket(packet.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, president_index);
    }
}

#ifndef RAFT_MODE
void Host::VicePresidentState() {
    for (int i = 0; i < num_hosts; ++i) {
        if (vp_hosts_responded_vector[i] && vp_hosts_log_index_vector[i] == vp_max_log_index) {
            uint16_t mask = 1 << i;

            vp_hosts_responded_bits |= mask;
            if (vp_hosts_success_vector[i]) {
                vp_hosts_success_bits |= mask;
            }
            else {
                vp_hosts_success_bits &= ~mask;
            }
            if (vp_hosts_isempty_vector[i]) {
                vp_hosts_is_empty_bits |= mask;
            }
            else {
                vp_hosts_is_empty_bits &= ~mask;
            }
        }
    }

    VpCombinedResponsePacket packet(vp_hosts_bits, vp_hosts_responded_bits,
        vp_hosts_success_bits, vp_hosts_max_term, vp_max_log_index);
    Network::SendPacket(packet.ToNetworkOrder().ToBytes(), SMALL_PACKET_SIZE, president_index);

    ChangeState(HostState::FOLLOWER);

    vp_hosts_responded_bits = 0;
    vp_hosts_success_bits = 0;
    vp_hosts_is_empty_bits = 0;
    vp_hosts_max_term = 0;
    vp_max_log_index = 0;
    for (int i = 0; i < num_hosts; ++i) {
        vp_hosts_isempty_vector[i] = false;
        vp_hosts_log_index_vector[i] = 0;
        vp_hosts_responded_vector[i] = false;
        vp_hosts_success_vector[i] = false;
    }
}
#endif
}

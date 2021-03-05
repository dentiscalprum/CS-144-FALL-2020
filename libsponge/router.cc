#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    // Your code here.
    _routing_table.insert(route_prefix, prefix_length, next_hop, interface_num);
    //_routing_table.debug_print();
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    // Your code here.
    auto &header = dgram.header();

    if(header.ttl <= 1) return;     // interger underflow
    header.ttl--;

    pair<optional<Address>, size_t> route_pair;
    if(_routing_table.find(header.dst, route_pair) == false) return;

    auto next_hop = route_pair.first.has_value() ? *(route_pair.first)
            : Address::from_ipv4_numeric(header.dst);
    interface(route_pair.second).send_datagram(dgram, next_hop);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

TrieNode::~TrieNode()
{
    for(int i = 0; i < 2; i++) {
        if(_nexts[i] != NULL) {
            delete(_nexts[i]);
        }
    }
}

void TrieNode::insert(const uint32_t route_prefix, const uint8_t prefix_length,
                        const optional<Address> next_hop, const size_t interface_num)
{
    size_t index = 31;
    TrieNode *current_node = this;
    for(uint8_t loop = 0; loop < prefix_length; loop++) {
        const uint32_t mask_bit = 1 << index;
        int next_loc = (route_prefix & mask_bit) >> index;
        cout << next_loc;
        if(current_node->_nexts[next_loc] == NULL) {
            current_node->_nexts[next_loc] = new TrieNode(32 - index);
        }
        current_node = current_node->_nexts[next_loc];
        index--;
    }

    current_node->_end = true;
    current_node->_next_hop = next_hop;
    current_node->_interface_num = interface_num;
}

bool TrieNode::find(const uint32_t ip, pair<optional<Address>, size_t> &result)
{
    bool is_find = false;

    TrieNode *current_node = this;
    for(int i = 31; i >= 0; i--) {
        if(current_node->_end == true) {
            is_find = true;
            result = make_pair(current_node->_next_hop, current_node->_interface_num);
        }
        if(i == 32) break;

        const uint32_t mask_bit = 1 << i;
        int next_loc = (ip & mask_bit) >> i;

        if(current_node->_nexts[next_loc] == NULL) {
            break;
        }
        current_node = current_node->_nexts[next_loc];
    }

    // if(is_find) {
    //     cerr << "\033[33m(find) next_hop: "
    //         << (result.first.has_value() ? result.first->to_string() : "(direct)")
    //         << ", interface_num: " << result.second << "\033[0m" << endl;
    // }
    return is_find;
}

void TrieNode::debug_print()
{
    if(_end) {
        cerr << "\033[35m(debug TrieNode)trie arrives at a end, next_hop: "
            << (_next_hop.has_value() ? _next_hop->to_string() : "(direct)")
            << ", prefix: " << _bit_loc
            << ", interface_num: " << _interface_num << "\033[0m" << endl;
    }
    for(int i = 0; i < 2; i++) {
        if(_nexts[i]) {
            _nexts[i]->debug_print();
        }
    }
}
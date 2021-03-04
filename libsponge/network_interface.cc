#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>
#include <stdio.h>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop)
{
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if(!_ip_map_ethernet.count(next_hop_ip)) {
        _unsend_datagram[next_hop_ip].push_back(dgram);
        if(!_unarp_ip_timer.count(next_hop_ip) || _cur_timer - _unarp_ip_timer[next_hop_ip] > 5 * 1000) {
            send_arp_frame(ARPMessage::OPCODE_REQUEST, ETHERNET_BROADCAST, next_hop_ip);
            _unarp_ip_timer[next_hop_ip] = _cur_timer;    
        }
        return;
    }

    send_ipv4_frame(dgram, _ip_map_ethernet[next_hop_ip]);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame)
{
    auto &header = frame.header();
    if(header.dst != _ethernet_address && header.dst != ETHERNET_BROADCAST) return std::nullopt;
    auto payload = frame.payload().concatenate();

    if(header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if(dgram.parse(std::move(payload)) != ParseResult::NoError) {
            return std::nullopt;
        }

        return dgram;
    } else if(header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_message;
        if(arp_message.parse(std::move(payload)) != ParseResult::NoError) {
            return std::nullopt;
        }

        auto target_ip_address = arp_message.sender_ip_address;
        auto target_ethernet_address = arp_message.sender_ethernet_address;
        if(arp_message.target_ip_address == _ip_address.ipv4_numeric()) {
            _start_timer_map_ip[_cur_timer].push_back(target_ip_address);
            _ip_map_ethernet[target_ip_address] = target_ethernet_address;

            clean_frame(target_ip_address);
            if(arp_message.opcode == ARPMessage::OPCODE_REQUEST) {
                send_arp_frame(ARPMessage::OPCODE_REPLY, target_ethernet_address, target_ip_address);
            }
        }
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
    _cur_timer += ms_since_last_tick;
    for(auto iter = _start_timer_map_ip.begin(); iter != _start_timer_map_ip.end(); ) {
        if(_cur_timer - iter->first < 30 * 1000) break;

        for(auto ip : iter->second) {
            _ip_map_ethernet.erase(ip);
        }
        
        _start_timer_map_ip.erase(iter++);
    }
}

void NetworkInterface::send_arp_frame(uint16_t opcode,
            EthernetAddress target_ethernet_address, uint32_t target_ip_address)
{
    ARPMessage arp_message;
    arp_message.opcode = opcode;
    arp_message.sender_ethernet_address = _ethernet_address;
    arp_message.sender_ip_address = _ip_address.ipv4_numeric();
    if(opcode == ARPMessage::OPCODE_REPLY) { 
        arp_message.target_ethernet_address = target_ethernet_address;
    }
    arp_message.target_ip_address = target_ip_address;

    EthernetFrame arp_frame;
    auto &frame_header = arp_frame.header();
    frame_header.dst = target_ethernet_address;
    frame_header.src = _ethernet_address;
    frame_header.type = EthernetHeader::TYPE_ARP;
    arp_frame.payload() = Buffer(arp_message.serialize());

    _frames_out.push(std::move(arp_frame));
}

void NetworkInterface::send_ipv4_frame(const InternetDatagram &dgram, EthernetAddress dst_ethernet_addr)
{
    EthernetFrame ipv4_frame;
    auto &header = ipv4_frame.header();
    header.dst = dst_ethernet_addr;
    header.src = _ethernet_address;
    header.type = EthernetHeader::TYPE_IPv4;
    ipv4_frame.payload() = dgram.serialize();

    _frames_out.push(std::move(ipv4_frame));
}

void NetworkInterface::clean_frame(uint32_t next_hop_ip)
{
    if(_unsend_datagram.count(next_hop_ip)) {
        auto next_hop_ethernet = _ip_map_ethernet[next_hop_ip];
        for(auto &dgram : _unsend_datagram[next_hop_ip]) {
            send_ipv4_frame(dgram, next_hop_ethernet);
        }

        _unsend_datagram.erase(next_hop_ip);
    }
}
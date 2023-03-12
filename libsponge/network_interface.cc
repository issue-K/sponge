#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

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

bool NetworkInterface::ShouldSendArp(const IP& ip) const {
    auto it = preArp_.find(ip);
    if (it == preArp_.end() || (*it).second) {
        return true;
    }
    return false;
}

bool NetworkInterface::CanIPToEthernet(const IP &ip) const {
    return mp_.find(ip) != mp_.end();
}

EthernetAddress NetworkInterface::IPToEthernet(const IP &ip) const {
    auto it = mp_.find(ip);
    // Todo: why not mp_[ip]?
    return (*it).second;
}

void NetworkInterface::DeleteIPMap(const IP& ip) {
    mp_.erase(ip);
    expired_mp_.erase(ip);
}

void NetworkInterface::AddIPMap(const IP& ip, const EthernetAddress& mac) {
    mp_[ip] = mac;
    expired_mp_[ip] = time_since_;
    SendWait();
}

void NetworkInterface::SendArp(const uint32_t& dst_ip, const EthernetAddress& mac, uint16_t op) {
    auto it = preArp_.find(dst_ip);
    if (it != preArp_.end() && it->second + ARP_INTERVAL > time_since_) {
        return;
    }
    preArp_[dst_ip] = time_since_;

    EthernetFrame e;
    e.header().src = _ethernet_address;
    e.header().dst = mac;
    e.header().type = EthernetHeader::TYPE_ARP;
    // 需要发送一个广播arp包来获取以太网地址
    ARPMessage arp;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    if (mac != ETHERNET_BROADCAST) {
        arp.target_ethernet_address = mac;
    }
    arp.target_ip_address = dst_ip;
    arp.opcode = op;
    e.payload() = arp.serialize();

    _frames_out.push(e);
}

void NetworkInterface::SendIPV4(const EthernetAddress &dst, const InternetDatagram &dgram) {
    EthernetFrame e;
    e.header().src = _ethernet_address;
    e.header().dst = dst;
    e.header().type = EthernetHeader::TYPE_IPv4;

    e.payload() = dgram.serialize();
    _frames_out.push(e);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
// 1、若已经知道ip对应的ethernet地址, 发送该ipv4包.
// 2、否则, 广播下一跳以太网地址的arp请求. 并保存ip数据包, 一遍后续得到映射后发送
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    auto next_hop_ip = next_hop.ipv4_numeric();
    if (!CanIPToEthernet(next_hop_ip)) {
        if (ShouldSendArp(dgram.header().dst)) {
            SendArp(next_hop_ip, ETHERNET_BROADCAST, ARPMessage::OPCODE_REQUEST);
        }
        wait_frames_.emplace(next_hop_ip, dgram);
        return;
    } else {
        SendIPV4(IPToEthernet(next_hop_ip), dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
// 1、收到ipv4帧直接返回
// 2、收到arp帧, 检查是否是发送给自己的. 如果是, 返回一个arp包.
//    并从中学习ip to ethernet的映射.
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    optional<InternetDatagram> res = nullopt;
    auto &dst = frame.header().dst;
    // 以太网地址不是自己
    if (dst != ETHERNET_BROADCAST && dst != _ethernet_address) {
        return res;
    }
    auto &header = frame.header();
    optional<InternetDatagram> data;
    if (header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipv4;
        if (ipv4.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            res.emplace(ipv4);
        }
    } else if (header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if (arp.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            AddIPMap(arp.sender_ip_address, arp.sender_ethernet_address);
            // 从arp中学习 ip to ethernet 的映射表
            if (arp.target_ip_address == _ip_address.ipv4_numeric()) {
                // 回复一个arp包
                SendArp(arp.sender_ip_address, arp.sender_ethernet_address, ARPMessage::OPCODE_REPLY);
            }
        }
    }
    return res;
}

void NetworkInterface::SendWait() {
    while (!wait_frames_.empty()) {
        const auto &it = wait_frames_.front();
        if (!CanIPToEthernet(it.first)) {
            break;
        }
        SendIPV4(IPToEthernet(it.first), it.second);
        wait_frames_.pop();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    time_since_ += ms_since_last_tick;
    // 查看正在等待的数据包是否能够发送
    // 使ip to ethernet的映射过期
    for(auto it = mp_.begin(); it != mp_.end();) {
        if (expired_mp_[it->first] + ARP_EXPIRE <= time_since_) {
            expired_mp_.erase(it->first);
            it = mp_.erase(it);
        } else {
            it++;
        }
    }
}

/**
 * @file socket.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "socket.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <netinet/in.h>

/**
 * @brief 
 * 
 * @param family 
 */
void UdpSocket::open(int family) {
    fd_ = socket(family, SOCK_DGRAM, 0);
    if (fd_ == -1) {
        throw std::runtime_error("socket() - failed");
    }
}

/**
 * @brief 
 * 
 */
UdpSocket::UdpSocket() {
    fd_ = -1;
    dest_addr_len_ = 0;
}

/**
 * @brief Destroy the Udp Socket:: Udp Socket object
 * 
 */
UdpSocket::~UdpSocket() {
    if (fd_ != -1) {
        close(fd_);
    }
}

/**
 * @brief 
 * 
 * @param address 
 * @param port 
 */
void UdpSocket::bind(const std::string& address, const std::string& port) {
    // When no address is specified, try dual-stack IPv6 first (accepts both IPv4 and IPv6).
    // This is needed so the receiver can accept connections from ::1 and 127.0.0.1.
    if (address.empty()) {
        fd_ = socket(AF_INET6, SOCK_DGRAM, 0);
        if (fd_ != -1) {
            int off = 0;
            setsockopt(fd_, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
            int enable = 1;
            setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

            struct sockaddr_in6 addr6{};
            addr6.sin6_family = AF_INET6;
            addr6.sin6_addr = in6addr_any;
            addr6.sin6_port = htons(static_cast<uint16_t>(std::stoi(port)));

            if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr6), sizeof(addr6)) == 0) {
                return; // dual-stack bind succeeded
            }
            // fall back to IPv4 below
            close(fd_);
            fd_ = -1;
        }
    }

    struct addrinfo hints{};
    struct addrinfo* result = nullptr;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // address == "" -> bind all

    const char* addr = nullptr;
    if (!address.empty()) {
        addr = address.c_str();
    }

    int rc = getaddrinfo(addr, port.c_str(), &hints, &result);
    if (rc != 0) {
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(rc));
    }
    open(result->ai_family);

    int enable = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    if (::bind(fd_, result->ai_addr, result->ai_addrlen) == -1) {
        freeaddrinfo(result);
        throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));
    }

    freeaddrinfo(result);
}

/**
 * @brief 
 * 
 * @param host 
 * @param port 
 */
void UdpSocket::resolve(const std::string& host, const std::string& port) {
    struct addrinfo hints{};
    struct addrinfo* result = nullptr;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (rc != 0) {
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(rc));
    }
    open(result->ai_family);

    std::memcpy(&dest_addr_, result->ai_addr, result->ai_addrlen);
    dest_addr_len_ = result->ai_addrlen;

    freeaddrinfo(result);
}

/**
 * @brief 
 * 
 * @param data 
 */
void UdpSocket::send(const std::vector<uint8_t>& data) {
    int rc = sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<const struct sockaddr*>(&dest_addr_), dest_addr_len_);
    if (rc == -1) {
        throw std::runtime_error(std::string("sendto() failed: ") + strerror(errno));
    }
}

/**
 * @brief
 *
 * @param data
 * @param dest_addr
 * @param dest_len
 */
void UdpSocket::send_to(const std::vector<uint8_t>& data, const struct sockaddr_storage& dest_addr, socklen_t dest_len) {
    int rc = sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<const struct sockaddr*>(&dest_addr), dest_len);
    if (rc == -1) {
        throw std::runtime_error(std::string("sendto() failed: ") + strerror(errno));
    }
}

/**
 * @brief 
 * 
 * @param buffer 
 * @param src_addr 
 * @param src_len 
 * @return size_t 
 */
size_t UdpSocket::receive(std::vector<uint8_t>& buffer, struct sockaddr_storage& src_addr, socklen_t& src_len) {
    buffer.resize(1500);

    int n = recvfrom(fd_, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&src_addr), &src_len);
    if (n == -1) {
        throw std::runtime_error(std::string("recvfrom() failed: ") + strerror(errno));
    }

    buffer.resize(n);
    return n;
}
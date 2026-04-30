/**
 * @file socket.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief UDP socket implementation with dual-stack bind and address resolution
 * @version 0.1
 * @date 2026-04-27
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. open() creates a UDP socket of the requested address family
 *   2. bind() tries dual-stack IPv6 first when no address is given,
 *      falls back to AF_UNSPEC getaddrinfo for explicit or IPv4-only systems
 *   3. resolve() resolves host:port via getaddrinfo and stores dest_addr_
 *   4. send() and send_to() wrap sendto() for client and server use respectively
 *   5. receive() wraps recvfrom() and resizes the buffer to actual received length
 */

#include "socket.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <netinet/in.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief opens a UDP socket of the given address family
 */
void UdpSocket::open(int family) {
    fd_ = socket(family, SOCK_DGRAM, 0);
    if (fd_ == -1) {
        throw std::runtime_error(std::string("socket() failed: ") + strerror(errno));
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief initialises the socket in a closed state
 */
UdpSocket::UdpSocket() : fd_(-1), dest_addr_{}, dest_addr_len_(0) {}

/**
 * @brief closes the socket if it was opened
 */
UdpSocket::~UdpSocket() {
    if (fd_ != -1) {
        close(fd_);
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief binds the socket to the given port (server side)
 */
void UdpSocket::bind(const std::string& address, const std::string& port) {
    // when no address is given, try dual-stack IPv6 first so the receiver
    // accepts both IPv4 and IPv6 connections on a single socket
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
                return;
            }
            // fall back to AF_UNSPEC
            close(fd_);
            fd_ = -1;
        }
    }

    struct addrinfo hints{};
    struct addrinfo* result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

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

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief resolves host:port and stores the destination address (client side)
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

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief sends data to the stored destination address
 */
void UdpSocket::send(const std::vector<uint8_t>& data) {
    int rc = sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<const struct sockaddr*>(&dest_addr_), dest_addr_len_);
    if (rc == -1) {
        throw std::runtime_error(std::string("sendto() failed: ") + strerror(errno));
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief sends data to an explicit destination address (server -> client)
 */
void UdpSocket::send_to(const std::vector<uint8_t>& data, const struct sockaddr_storage& dest_addr, socklen_t dest_len) {
    int rc = sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<const struct sockaddr*>(&dest_addr), dest_len);
    if (rc == -1) {
        throw std::runtime_error(std::string("sendto() failed: ") + strerror(errno));
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief receives one datagram and fills the source address
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
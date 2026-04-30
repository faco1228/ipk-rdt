/**
 * @file socket.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief RAII wrapper around a UDP socket with address resolution and dual-stack support
 * @version 0.1
 * @date 2026-04-27
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. UdpSocket owns a single UDP file descriptor and closes it in the destructor
 *   2. bind() is used by the receiver — tries dual-stack IPv6 first, falls back to IPv4
 *   3. resolve() is used by the sender — resolves host:port and stores dest_addr_
 *   4. send() sends to stored dest_addr_, send_to() sends to an explicit address
 *   5. receive() wraps recvfrom() and fills src_addr for reply addressing
 */

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdint>
#include <vector>
#include <string>

/**
 * @brief RAII wrapper around a UDP socket
 *
 * non-copyable — the file descriptor is a unique resource
 */
class UdpSocket {
public:
    /**
     * @brief initialises the socket in a closed state (fd_ = -1)
     */
    explicit UdpSocket();

    /**
     * @brief closes the socket if it was opened
     */
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    /**
     * @brief binds the socket to the given port (server side)
     *
     * when address is empty, tries dual-stack IPv6 first (IPV6_V6ONLY=0)
     * so the receiver accepts both IPv4 and IPv6 on one socket
     * falls back to AF_UNSPEC getaddrinfo if IPv6 is not available
     *
     * @param address local address to bind to, or empty for all interfaces
     * @param port local port number as string
     * @throws std::runtime_error if bind fails
     */
    void bind(const std::string& address, const std::string& port);

    /**
     * @brief resolves host:port and stores the destination address (client side)
     *
     * @param host destination hostname or IP address
     * @param port destination port number as string
     * @throws std::runtime_error if resolution fails
     */
    void resolve(const std::string& host, const std::string& port);

    /**
     * @brief sends data to the stored destination address
     *
     * @param data bytes to send
     * @throws std::runtime_error if sendto fails
     */
    void send(const std::vector<uint8_t>& data);

    /**
     * @brief sends data to an explicit destination address (server -> client)
     *
     * @param data bytes to send
     * @param dest_addr destination address
     * @param dest_len length of dest_addr
     * @throws std::runtime_error if sendto fails
     */
    void send_to(const std::vector<uint8_t>& data, const struct sockaddr_storage& dest_addr, socklen_t dest_len);

    /**
     * @brief receives one datagram and fills the source address
     *
     * @param buffer filled with received bytes (resized to actual length)
     * @param src_addr filled with the sender's address
     * @param src_len filled with the length of src_addr
     * @return number of bytes received
     * @throws std::runtime_error if recvfrom fails
     */
    size_t receive(std::vector<uint8_t>& buffer, struct sockaddr_storage& src_addr, socklen_t& src_len);

    int fd() const { return fd_; } // file descriptor, used for poll()

private:
    int fd_;
    struct sockaddr_storage dest_addr_; // stored destination for send()
    socklen_t dest_addr_len_;

    /**
     * @brief opens a UDP socket of the given address family
     *
     * @param family AF_INET or AF_INET6
     * @throws std::runtime_error if socket() fails
     */
    void open(int family);
};

#endif // SOCKET_HPP
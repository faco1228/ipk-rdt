/**
 * @file socket.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netdb.h>       // getaddrinfo
#include <unistd.h>      // close
#include <cstdint>
#include <vector>

class UdpSocket {
public:
    /**
     * @brief Construct a new Udp Socket object
     * 
     */
    explicit UdpSocket(); // default, fd_ = -1
    
    /**
     * @brief Destroy the Udp Socket object
     * 
     */
    ~UdpSocket(); // close(fd_), fd_ != -1

    // non-copyable (RAII rule)

    /**
     * @brief Construct a new Udp Socket object
     * 
     */
    UdpSocket(const UdpSocket&) = delete;

    /**
     * @brief Construct a new operator = object
     * 
     */
    UdpSocket& operator=(const UdpSocket&) = delete;

    /**
     * @brief 
     * server: getaddrinfo + socket() + SO_REUSEADDR + bind()
     * @param address 
     * @param port 
     */
    void bind(const std::string& address, const std::string& port);

    /**
     * @brief 
     * klient: getaddrinfo, saves dest_addr_
     * @param host 
     * @param port 
     */
    void resolve(const std::string& host, const std::string& port);

    /**
     * @brief 
     * sendto dest_addr_
     * @param data 
     */
    void send(const std::vector<uint8_t>& data);

    /**
     * @brief 
     * sendto actual adress (for server -> klient)
     * @param data 
     * @param dest_addr 
     * @param addrlen 
     */
    void send_to(const std::vector<uint8_t>& data, const struct sockaddr_storage& dest_addr, socklen_t dest_len);

    /**
     * @brief 
     * recvfrom, returns number of bytes received
     * @param buffer 
     * @param src_addr 
     * @param addrlen 
     * @return size_t 
     */
    size_t receive(std::vector<uint8_t>& buffer, struct sockaddr_storage& src_addr, socklen_t& src_len);

    // getter, for poll()
    int fd() const { return fd_; }

private:
    // socket file descriptor
    int fd_ = -1;

    // stored destination address (used by send())
    struct sockaddr_storage dest_addr_;

    // length of destination address
    socklen_t dest_addr_len_;

    /**
     * @brief 
     * internal: socket(family, SOCK_DGRAM, 0)
     * @param family 
     */
    void open(int family);
};

#endif // SOCKET_HPP
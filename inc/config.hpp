/**
 * @file config.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief program configuration struct and mode enum
 * @version 0.1
 * @date 2026-04-15
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. Mode enum selects between CLIENT and SERVER operation
 *   2. Config holds all parsed CLI options passed to Sender or Receiver
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <cstdint>

/**
 * @brief selects the operating mode of the program
 */
enum class Mode {
    CLIENT,
    SERVER
};

/**
 * @brief holds all parsed CLI options
 */
struct Config {
    Mode mode;
    uint16_t port = 0;
    std::string address = ""; // bind address (server) or destination host (client)
    std::string file_in = ""; // input file path, empty means stdin
    std::string file_out = ""; // output file path, empty means stdout
    int timeout = 1; // progress and handshake timeout in seconds
};

#endif // CONFIG_HPP
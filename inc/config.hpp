/**
 * @file config.hpp
 * @author Samuel Facka (xfackas00)
 * @brief 
 * @version 0.1
 * @date 2026-04-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <cstdint>



enum class Mode {
    CLIENT,
    SERVER
};

struct Config {
    Mode mode;
    uint16_t port = 0;
    std::string address = "";  // bind addr - server / host - client
    std::string file_in = "";  // input filepath, "" if stdin
    std::string file_out = ""; // output filepath, "" if stdout
    int timeout = 1;           // default: 1
};

#endif // CONFIG_HPP
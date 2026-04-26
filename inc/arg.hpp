/**
 * @file arg.hpp
 * @author Samuel Facka (xfackas00)
 * @brief 
 * @version 0.1
 * @date 2026-04-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef ARG_HPP
#define ARG_HPP

#include "config.hpp"
#include <iostream>

class ArgParser {
    public: 
        Config parse(int argc, char* argv[]) const;
        void print_help(std::ostream& os) const;
    private:
        uint16_t parse_port(const std::string& port_s) const;
        int parse_timeout(const std::string& timeout_s) const;
        void validate_config(const Config& config) const;
};

#endif // ARG_HPP
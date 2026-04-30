/**
 * @file arg.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief CLI argument parser
 * @version 0.1
 * @date 2026-04-15
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. parse() iterates argv, recognises all supported flags and builds a Config
 *   2. validate_config() checks that required fields are present and consistent
 *   3. parse_port() and parse_timeout() convert strings to numbers with range checks
 *   4. print_help() prints usage to the given output stream and returns
 */

#ifndef ARG_HPP
#define ARG_HPP

#include "config.hpp"
#include <iostream>

/**
 * @brief parses CLI arguments and returns a validated Config
 */
class ArgParser {
public:
    /**
     * @brief parses argc/argv and returns valida Config
     *
     * @param argc argument count from main
     * @param argv argument vector from main
     * @return validated Config
     * @throws std::invalid_argument if arguments are invalid or missing
     */
    Config parse(int argc, char* argv[]) const;

    /**
     * @brief prints usage information to the given output stream
     *
     * @param os output stream to write to
     */
    void print_help(std::ostream& os) const;

private:
    /**
     * @brief converts a string to a port number and validates the range
     *
     * @param port_s port number as string
     * @return port number as uint16_t
     * @throws std::runtime_error if value is not a number or out of range 1-65535
     */
    uint16_t parse_port(const std::string& port_s) const;

    /**
     * @brief converts a string to a timeout value and validates it
     *
     * @param timeout_s timeout as string
     * @return timeout in seconds
     * @throws std::runtime_error if value is not a positive integer
     */
    int parse_timeout(const std::string& timeout_s) const;

    /**
     * @brief checks that the Config is consistent and all required fields are set
     *
     * @param config config to validate
     * @throws std::runtime_error if port is missing or client has no address
     */
    void validate_config(const Config& config) const;
};

#endif // ARG_HPP
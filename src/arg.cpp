/**
 * @file arg.cpp
 * @author Samuel Facka (xfackas00)
 * @brief 
 * @version 0.1
 * @date 2026-04-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "arg.hpp"

Config ArgParser::parse(int argc, char* argv[]) const {
    Config config;
    bool seen_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_help(std::cout);
            exit(0);
        }
        else if (arg == "-c") {
            config.mode = Mode::CLIENT;
            seen_mode = true;
        }
        else if (arg == "-s") {
            config.mode = Mode::SERVER;
            seen_mode = true;
        }
        else if (arg == "-p") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Expected port number after -p");
            }
            config.port = parse_port(argv[++i]);
        }
        else if (arg == "-a") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Expected address after -a");
            }
            config.address = argv[++i];
        }
        else if (arg == "-i") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Expected input file path after -i");
            }
            config.file_in = argv[++i];
        }
        else if (arg == "-o") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Expected output file path after -o");
            }
            config.file_out = argv[++i];
        }
        else if (arg == "-w") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Expected timeout value after -w");
            }
            config.timeout = parse_timeout(argv[++i]);
        }
        else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (!seen_mode) {
        throw std::runtime_error("Mode not specified. Use -c for client or -s for server");
    }

    validate_config(config);
    return config;
}

uint16_t ArgParser::parse_port(const std::string& port_s) const {
    try {
        int port = std::stoi(port_s);
        if (port < 1 || port > 65535) {
            throw std::runtime_error("Port number must be between 1 and 65535");
        }
        return static_cast<uint16_t>(port);
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Invalid port number: " + port_s);
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Port number out of range: " + port_s);
    }
}

int ArgParser::parse_timeout(const std::string& timeout_s) const {
    try {
        int timeout = std::stoi(timeout_s);
        if (timeout < 1) {
            throw std::runtime_error("Timeout must be a positive integer");
        }
        return timeout;
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Invalid timeout value: " + timeout_s);
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Timeout value out of range: " + timeout_s);
    }
}

void ArgParser::validate_config(const Config& config) const {
    if (config.port == 0) {
        throw std::runtime_error("Port not specified (-p PORT)");
    }
    if (config.mode == Mode::CLIENT && config.address.empty()) {
        throw std::runtime_error("Client mode requires address (-a ADDRESS)");
    }
}

void ArgParser::print_help(std::ostream& os) const {
    os << "Usage:\n"
       << "  Server: ./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT]\n"
       << "  Client: ./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT]\n"
       << "\nOptions:\n"
       << "  -s            Server mode\n"
       << "  -c            Client mode\n"
       << "  -p PORT       UDP port number\n"
       << "  -a ADDRESS    Bind address (server) or destination host (client)\n"
       << "  -i INPUT      Input file (client), default: stdin\n"
       << "  -o OUTPUT     Output file (server), default: stdout\n"
       << "  -w TIMEOUT    Timeout in seconds, default: 1\n"
       << "  -h, --help    Show this help\n";
}
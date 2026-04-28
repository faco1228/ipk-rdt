/**
 * @file main.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <iostream> 
#include <csignal> 
#include <stdexcept>

#include "arg.hpp"
#include "sender.hpp"
#include "receiver.hpp"

volatile sig_atomic_t g_interrupted = 0;

static void handle_signal(int signum) {
    (void)signum;
    g_interrupted = 1;
}

int main(int argc, char* argv[]) {
    struct sigaction sig_action;
    sig_action.sa_handler = handle_signal;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;

    sigaction(SIGINT, &sig_action, nullptr);
    sigaction(SIGTERM, &sig_action, nullptr);

    try {
        ArgParser parser;
        Config config = parser.parse(argc, argv);

        if (config.mode == Mode::CLIENT) {
            // Sender sender(config);
            // sender.run();
        }
        else {
            // Receiver receiver(config);
            // receiver.run();
        }

        return 0;
    }
    catch (const std::invalid_argument& error) {
        std::cerr << "Argument error: " << error.what() << "\n";
        return 1;
    }
    catch (const std::runtime_error& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown error\n";
        return 1; 
    }
}
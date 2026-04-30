/**
 * @file main.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief program entry point: signal handling, argument parsing and mode dispatch
 * @version 0.1
 * @date 2026-04-28
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. SIGINT and SIGTERM are caught via sigaction and set g_interrupted
 *   2. ArgParser parses and validates all CLI arguments into a Config
 *   3. based on Config.mode, either Sender or Receiver is constructed and run()
 *   4. exceptions are caught and printed to stderr with exit code 1
 */

#include <iostream>
#include <csignal>
#include <stdexcept>

#include "arg.hpp"
#include "sender.hpp"
#include "receiver.hpp"

volatile sig_atomic_t g_interrupted = 0; // set to 1 by SIGINT/SIGTERM handler

/**
 * @brief signal handler for SIGINT and SIGTERM
 *
 * sets g_interrupted to 1
 * the main loop checks this flag to exit cleanly
 *
 * @param signum received signal number (unused)
 */
static void handle_signal(int signum) {
    (void)signum;
    g_interrupted = 1;
}

/**
 * @brief program entry point
 *
 * @param argc argument count
 * @param argv argument vector
 * @return 0 on success, 1 on any error
 */
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
            Sender sender(config);
            sender.run();
        }
        else {
            Receiver receiver(config);
            receiver.run();
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
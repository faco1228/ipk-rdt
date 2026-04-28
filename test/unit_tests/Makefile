# Makefile for unit tests
# Samuel Facka (xfackas00)

CXX      = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -pedantic -I../../inc

SRC_DIR  = ../../src
BUILD_DIR = build

TESTS = test_packet

.PHONY: all clean $(TESTS)

all: $(TESTS)

test_packet: $(BUILD_DIR)/test_packet
	./$(BUILD_DIR)/test_packet

$(BUILD_DIR)/test_packet: test_packet.cpp $(SRC_DIR)/packet.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

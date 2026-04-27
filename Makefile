# Makefile for IPK - Reliable File Transfer over UDP
# Samuel Facka (xfackas00)

CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -pedantic -Iinc -MMD -MP

SRC_DIR = src
INC_DIR = inc
TEST_DIR = test
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

TARGET = ipk-rdt
ZIP_NAME = xfackas00.zip

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean purge re test zip NixDevShellName

all: $(TARGET)

re: purge all

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

NixDevShellName:
	@echo c

clean:
	rm -rf $(BUILD_DIR) test_output.txt

purge: clean
	rm -f $(TARGET) $(ZIP_NAME)

test: $(TARGET)
	chmod +x $(TEST_DIR)/test.sh
	./$(TEST_DIR)/test.sh

zip:
	zip -r $(ZIP_NAME) src inc test Makefile README.md CHANGELOG.md LICENSE

-include $(DEPS)
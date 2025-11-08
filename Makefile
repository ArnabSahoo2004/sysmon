CXX      := g++
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -Wshadow -pedantic
SRC_DIR  := src
BIN_DIR  := bin
TARGET   := $(BIN_DIR)/sysmon
SRCS     := $(SRC_DIR)/main.cpp

all: $(TARGET)

$(TARGET): $(SRCS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean

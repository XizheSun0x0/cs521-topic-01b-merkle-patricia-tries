# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++14 -O0 -g -Wall

# Target binary name
TARGET = mpt_test*

# Source files
SRCS = merkle_patricia_trie.cpp

# Default rule: build the binary
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# Rule to build and run immediately
run: $(TARGET)
	./$(TARGET)

# Rule to clean up build artifacts
clean:
	rm -rf $(TARGET)
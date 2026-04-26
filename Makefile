CXX      ?= clang++
CXXFLAGS  = -std=c++14 -O2 -Wall

.PHONY: all clean server localtest benchmark

all: localtest server benchmark

localtest: localtest.cpp merkle_patricia_trie.h
	$(CXX) $(CXXFLAGS) -o localtest localtest.cpp

server: server.cpp merkle_patricia_trie.h
	$(CXX) $(CXXFLAGS) -o server server.cpp
# benchmark mpt performance
benchmark: benchmark.cpp merkle_patricia_trie.h
	$(CXX) $(CXXFLAGS) -o benchmark benchmark.cpp
clean:
	rm -f localtest server benchmark

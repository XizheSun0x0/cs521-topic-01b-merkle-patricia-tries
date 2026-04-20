CXX      ?= clang++
CXXFLAGS  = -std=c++14 -O2 -Wall

.PHONY: all clean server localtest

all: localtest server

localtest: localtest.cpp merkle_patricia_trie.h
	$(CXX) $(CXXFLAGS) -o localtest localtest.cpp

server: server.cpp merkle_patricia_trie.h
	$(CXX) $(CXXFLAGS) -o server server.cpp

clean:
	rm -f localtest server

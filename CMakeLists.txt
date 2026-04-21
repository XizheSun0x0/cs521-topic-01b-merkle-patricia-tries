cmake_minimum_required(VERSION 3.10)
project(MerklePatriciaTrie LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── localtest: standalone test runner ──
add_executable(localtest localtest.cpp)

# ── server: HTTP API server for the Vue frontend ──
add_executable(server server.cpp)

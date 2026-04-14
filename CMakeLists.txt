cmake_minimum_required(VERSION 3.21)

# set your project name here
set(PROJECT_NAME "merkle_patricia_tries")

project(${PROJECT_NAME} LANGUAGES CXX)

# modify cpp std version here
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add cpp resources files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "merkle_patricia_trie.cpp")

# add executable target
set(EXE_NAME "test_app") # or set(EXE_NAME ${PROJECT_NAME})
add_executable(${EXE_NAME} ${SRC_FILES})


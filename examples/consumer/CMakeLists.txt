cmake_minimum_required(VERSION 3.21)
project(kprotocol_consumer LANGUAGES CXX)

find_package(kprotocol CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE kprotocol::kprotocol)

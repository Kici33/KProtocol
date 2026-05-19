cmake_minimum_required(VERSION 3.21)
project(kprotocol_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example src/main.cpp)

# Build the kprotocol library from the parent directory (adjust path if kprotocol is elsewhere)
# This adds the top-level project as a subdirectory and builds the kprotocol target.
add_subdirectory(${CMAKE_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/kprotocol_build)

target_link_libraries(example PRIVATE kprotocol)

target_include_directories(example PRIVATE ${CMAKE_SOURCE_DIR}/../include)

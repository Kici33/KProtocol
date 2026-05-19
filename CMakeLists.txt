cmake_minimum_required(VERSION 3.21)
project(kprotocol VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kprotocol
    src/codec.cpp
    src/types.cpp
    src/registry.cpp
    src/translation.cpp
    src/translation_registry.cpp
    src/server.cpp
    src/baseline_packets.cpp
)

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated")
# Use BUILD_INTERFACE/INSTALL_INTERFACE so exported targets don't reference the source tree directly
target_include_directories(kprotocol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/kprotocol>
    $<INSTALL_INTERFACE:include>
)

if(EXISTS "${GENERATED_DIR}")
    target_include_directories(kprotocol PUBLIC $<BUILD_INTERFACE:${GENERATED_DIR}>)
endif()

target_compile_features(kprotocol PUBLIC cxx_std_20)

# If generator output exists, include generated registration sources
if(EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp")
    message(STATUS "Including generated packet registry")
    target_sources(kprotocol PRIVATE ${GENERATED_DIR}/registry/register_all_packets.cpp)
endif()

if(MSVC)
    target_compile_options(kprotocol PRIVATE /W4 /permissive- /std:c++latest)
else()
    target_compile_options(kprotocol PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link zlib for compression support
find_package(ZLIB REQUIRED)
target_link_libraries(kprotocol PRIVATE ZLIB::ZLIB)

if(WIN32)
    target_link_libraries(kprotocol PRIVATE ws2_32)
endif()

# Don't build example by default in CI if jthread isn't available
option(BUILD_EXAMPLE "Build example server" OFF)
if(BUILD_EXAMPLE)
    add_executable(kprotocol_example examples/minimal_server.cpp)
    target_link_libraries(kprotocol_example PRIVATE kprotocol)
endif()

enable_testing()
add_executable(kprotocol_tests tests/codec_tests.cpp)
target_link_libraries(kprotocol_tests PRIVATE kprotocol)
add_test(NAME kprotocol_tests COMMAND kprotocol_tests)

add_executable(kprotocol_translation_tests tests/translation_tests.cpp)
target_link_libraries(kprotocol_translation_tests PRIVATE kprotocol)
add_test(NAME kprotocol_translation_tests COMMAND kprotocol_translation_tests)

add_executable(kprotocol_compression_tests tests/compression_tests.cpp)
target_link_libraries(kprotocol_compression_tests PRIVATE kprotocol)
add_test(NAME kprotocol_compression_tests COMMAND kprotocol_compression_tests)

add_executable(kprotocol_integration_tests tests/integration_tests.cpp)
target_link_libraries(kprotocol_integration_tests PRIVATE kprotocol)
add_test(NAME kprotocol_integration_tests COMMAND kprotocol_integration_tests)


# Installation rules
install(TARGETS kprotocol
    EXPORT kprotocolTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install public headers
install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")

# Install generated headers (if present) under include/kprotocol/generated
if(EXISTS "${GENERATED_DIR}")
    file(GLOB_RECURSE GENERATED_HEADERS "${GENERATED_DIR}/*.hpp")
    if(GENERATED_HEADERS)
        install(FILES ${GENERATED_HEADERS} DESTINATION include/kprotocol/generated)
    endif()
endif()

# Export CMake targets for downstream projects
install(EXPORT kprotocolTargets FILE kprotocolTargets.cmake NAMESPACE kprotocol:: DESTINATION lib/cmake/kprotocol)


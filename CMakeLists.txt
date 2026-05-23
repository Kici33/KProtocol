cmake_minimum_required(VERSION 3.21)
project(kprotocol VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------- Options --------------------------------------------
option(KPROTOCOL_BUILD_TESTS    "Build kprotocol unit + integration tests" ON)
option(KPROTOCOL_BUILD_EXAMPLES "Build kprotocol example binaries"         OFF)
option(KPROTOCOL_BUILD_GENERATED_PACKETS
       "Compile the committed generated/ packet catalog into kprotocol_packets" ON)
option(BUILD_EXAMPLE            "(deprecated) alias of KPROTOCOL_BUILD_EXAMPLES" OFF)
if(BUILD_EXAMPLE)
    set(KPROTOCOL_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
endif()

set(KPROTOCOL_GENERATE_VERSIONS "1.8,1.12.2,1.16.5,1.20.4,1.21.1"
    CACHE STRING
    "Comma-separated Minecraft versions to pass to tools/generate_packets.mjs (used by the kprotocol_generate_packets target)")

find_package(ZLIB REQUIRED)

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated")

function(kprotocol_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ============================================================================
# kprotocol_codec: pure wire codec + version table + key interner.
# No knowledge of packets, networking, or registries.
# ============================================================================
add_library(kprotocol_codec
    src/codec.cpp
    src/version.cpp
    src/internal/packet_key.cpp
)
target_compile_features(kprotocol_codec PUBLIC cxx_std_20)
target_include_directories(kprotocol_codec PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(kprotocol_codec PUBLIC ZLIB::ZLIB)
kprotocol_apply_warnings(kprotocol_codec)
add_library(kprotocol::codec ALIAS kprotocol_codec)

# ============================================================================
# kprotocol_core: PacketRegistry, PacketTranslator, type registry. Depends only
# on the codec for VarInt/string IO.
# ============================================================================
add_library(kprotocol_core
    src/types.cpp
    src/registry.cpp
    src/translation.cpp
    src/translation_registry.cpp
)
target_link_libraries(kprotocol_core PUBLIC kprotocol_codec)
kprotocol_apply_warnings(kprotocol_core)
add_library(kprotocol::core ALIAS kprotocol_core)

# ============================================================================
# kprotocol_packets: baseline packet definitions + (optional) generated catalog.
# This is where the per-version PacketDefinition tables live.
# ============================================================================
add_library(kprotocol_packets
    src/baseline_packets.cpp
)
target_link_libraries(kprotocol_packets PUBLIC kprotocol_core)

# The committed generator output lives under generated/include and
# generated/registry. Header layout: generated/include/kprotocol/generated/*.hpp
# so a single -I generated/include exposes them at #include "kprotocol/generated/...".
if(KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/include")
    target_include_directories(kprotocol_packets PUBLIC
        $<BUILD_INTERFACE:${GENERATED_DIR}/include>
    )
endif()
if(KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp")
    message(STATUS "kprotocol_packets: including generated registry source")
    target_sources(kprotocol_packets PRIVATE ${GENERATED_DIR}/registry/register_all_packets.cpp)
    target_compile_definitions(kprotocol_packets PUBLIC KPROTOCOL_HAS_GENERATED_CATALOG=1)
endif()
kprotocol_apply_warnings(kprotocol_packets)
add_library(kprotocol::packets ALIAS kprotocol_packets)

# ============================================================================
# kprotocol_generate_packets: convenience target that re-runs the generator
# from minecraft-data. Requires Node.js + `npm install minecraft-data` in the
# project root. Not built by default - invoke with
#     cmake --build . --target kprotocol_generate_packets
# ============================================================================
find_program(KPROTOCOL_NODE_EXECUTABLE NAMES node nodejs)
if(KPROTOCOL_NODE_EXECUTABLE)
    add_custom_target(kprotocol_generate_packets
        COMMAND ${KPROTOCOL_NODE_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/generate_packets.mjs
                --out      ${GENERATED_DIR}
                --versions ${KPROTOCOL_GENERATE_VERSIONS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Regenerating packet catalog into ${GENERATED_DIR} (versions: ${KPROTOCOL_GENERATE_VERSIONS})"
        VERBATIM
    )
endif()

# ============================================================================
# kprotocol_server: TCP accept loop + ClientSession. Optional - users embedding
# the codec in their own I/O stack can omit it.
# ============================================================================
add_library(kprotocol_server
    src/server.cpp
)
target_link_libraries(kprotocol_server PUBLIC kprotocol_core)
if(WIN32)
    target_link_libraries(kprotocol_server PRIVATE ws2_32)
endif()
kprotocol_apply_warnings(kprotocol_server)
add_library(kprotocol::server ALIAS kprotocol_server)

# ============================================================================
# Umbrella INTERFACE target. Existing `target_link_libraries(... kprotocol)`
# call sites keep working; new code is encouraged to depend on the specific
# sub-target it needs.
# ============================================================================
add_library(kprotocol INTERFACE)
target_link_libraries(kprotocol INTERFACE
    kprotocol_codec
    kprotocol_core
    kprotocol_packets
    kprotocol_server
)
add_library(kprotocol::kprotocol ALIAS kprotocol)

# ============================================================================
# Examples
# ============================================================================
if(KPROTOCOL_BUILD_EXAMPLES)
    add_executable(kprotocol_example examples/minimal_server.cpp)
    target_link_libraries(kprotocol_example PRIVATE kprotocol::kprotocol)
endif()

# ============================================================================
# Tests
# ============================================================================
if(KPROTOCOL_BUILD_TESTS)
    enable_testing()

    set(KPROTOCOL_TEST_NAMES
        codec_tests
        translation_tests
        compression_tests
        integration_tests
        varint_property_tests
        frame_malformed_tests
        version_tests
        decompression_bomb_tests
        field_types_tests
        versioned_schema_tests
        generated_packets_tests
    )

    foreach(test_name IN LISTS KPROTOCOL_TEST_NAMES)
        # Tests that reference register_generated_packets need the committed
        # generator output to be compiled in. Skip them if the user opted out.
        if(test_name STREQUAL "generated_packets_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        set(test_path "${CMAKE_SOURCE_DIR}/tests/${test_name}.cpp")
        if(EXISTS "${test_path}")
            add_executable(kprotocol_${test_name} ${test_path})
            target_link_libraries(kprotocol_${test_name} PRIVATE kprotocol::kprotocol)
            add_test(NAME kprotocol_${test_name} COMMAND kprotocol_${test_name})
        endif()
    endforeach()
endif()

# ============================================================================
# Install / export
# ============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS
        kprotocol kprotocol_codec kprotocol_core kprotocol_packets kprotocol_server
    EXPORT kprotocolTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")

if(KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/include")
    install(DIRECTORY ${GENERATED_DIR}/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.hpp")
endif()

install(EXPORT kprotocolTargets
    FILE      kprotocolTargets.cmake
    NAMESPACE kprotocol::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kprotocol
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfig.cmake"
"include(CMakeFindDependencyMacro)
find_dependency(ZLIB)
include(\"\${CMAKE_CURRENT_LIST_DIR}/kprotocolTargets.cmake\")
")

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kprotocol
)

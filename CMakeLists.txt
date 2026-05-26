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

set(KPROTOCOL_GENERATE_VERSIONS "1.8,1.12.2,1.13,1.14,1.16.5,1.17,1.18,1.19,1.20.2,1.20.4,1.21.1,1.21.4,1.21.5,1.21.6,1.21.7,1.21.9,1.21.11"
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
# kprotocol_codec: pure wire codec + version table. No packets or registries.
# ============================================================================
add_library(kprotocol_codec
    src/codec.cpp
    src/version.cpp
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
    src/text_component.cpp
    src/internal/packet_key.cpp
    src/registry.cpp
    src/translation.cpp
    src/translation_registry.cpp
    src/translation_mappings_data.cpp
    src/entity_metadata.cpp
    src/blocks.cpp
    src/block_registry.cpp
    src/metadata_registry.cpp
    src/block_registry_data.cpp
    src/metadata_registry_data.cpp
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
    src/initialize.cpp
    src/runtime.cpp
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
    add_custom_target(kprotocol_embed_translation_mappings
        COMMAND ${KPROTOCOL_NODE_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/embed_translation_mappings.mjs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Embedding tools/translation_mappings.json into src/translation_mappings_data.cpp"
        VERBATIM
    )
endif()

# ============================================================================
# Asio (standalone, header-only) — used only by kprotocol_server.
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    kprotocol_asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_GetProperties(kprotocol_asio)
if(NOT kprotocol_asio_POPULATED)
    FetchContent_MakeAvailable(kprotocol_asio)
    if(NOT TARGET kprotocol_asio)
        add_library(kprotocol_asio INTERFACE)
        target_include_directories(kprotocol_asio SYSTEM INTERFACE
            $<BUILD_INTERFACE:${kprotocol_asio_SOURCE_DIR}/asio/include>)
        target_compile_definitions(kprotocol_asio INTERFACE ASIO_STANDALONE)
        if(WIN32)
            target_compile_definitions(kprotocol_asio INTERFACE _WIN32_WINNT=0x0A00)
        endif()
    endif()
    add_library(kprotocol::asio ALIAS kprotocol_asio)
endif()

# ============================================================================
# kprotocol_server: Asio coroutine TCP runtime + ClientSession.
# ============================================================================
add_library(kprotocol_server
    src/server.cpp
    src/client_ui.cpp
    src/play_demo.cpp
    src/protocol_server.cpp
)
target_link_libraries(kprotocol_server PUBLIC kprotocol_core kprotocol_asio)
if(WIN32)
    target_link_libraries(kprotocol_server PRIVATE ws2_32 mswsock)
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
    add_executable(kprotocol_play_demo examples/play_demo_server.cpp)
    target_link_libraries(kprotocol_play_demo PRIVATE kprotocol::kprotocol)
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
        version_range_tests
        decompression_bomb_tests
        field_types_tests
        versioned_schema_tests
        generated_packets_tests
        server_smoke_tests
        server_login_tests
        gameplay_integration_tests
        server_play_demo_tests
        blocks_metadata_tests
        easy_use_tests
        ui_tests
    )

    foreach(test_name IN LISTS KPROTOCOL_TEST_NAMES)
        # Tests that reference register_generated_packets need the committed
        # generator output to be compiled in. Skip them if the user opted out.
        if(test_name STREQUAL "generated_packets_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "version_range_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "server_login_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "gameplay_integration_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "server_play_demo_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "blocks_metadata_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "easy_use_tests"
                AND NOT (KPROTOCOL_BUILD_GENERATED_PACKETS AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
            continue()
        endif()
        if(test_name STREQUAL "ui_tests"
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
        kprotocol kprotocol_codec kprotocol_core kprotocol_packets kprotocol_server kprotocol_asio
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
# kprotocol::asio is exported alongside the library targets (standalone Asio).
# Consumers linking kprotocol::server or kprotocol::kprotocol receive it transitively.
")

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kprotocol
)

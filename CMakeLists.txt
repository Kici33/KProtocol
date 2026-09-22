cmake_minimum_required(VERSION 3.21)
project(kprotocol VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------- Options --------------------------------------------
set(KPROTOCOL_BUILD_TESTS_DEFAULT ${PROJECT_IS_TOP_LEVEL})
option(KPROTOCOL_BUILD_TESTS    "Build kprotocol unit + integration tests" ${KPROTOCOL_BUILD_TESTS_DEFAULT})
option(KPROTOCOL_BUILD_EXAMPLES "Build kprotocol example binaries"         OFF)
option(KPROTOCOL_BUILD_AUTH "Build Microsoft/Minecraft authentication and HTTPS support" OFF)
option(KPROTOCOL_BUILD_CLIENT "Build the outbound protocol-47 Minecraft client (includes auth)" OFF)
if(KPROTOCOL_BUILD_CLIENT)
    set(KPROTOCOL_BUILD_AUTH ON)
endif()
option(KPROTOCOL_BUILD_REAL_CLIENT_TESTS
       "Add opt-in tests that run an external real Minecraft client against kprotocol_play_demo" OFF)
option(KPROTOCOL_BUILD_GENERATED_PACKETS
       "Compile the committed generated/ packet catalog into kprotocol_packets" ON)
option(KPROTOCOL_FETCH_ASIO
       "Download standalone Asio with CMake FetchContent when KPROTOCOL_ASIO_SOURCE_DIR is not set" ON)
option(BUILD_EXAMPLE            "(deprecated) alias of KPROTOCOL_BUILD_EXAMPLES" OFF)
if(BUILD_EXAMPLE)
    set(KPROTOCOL_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
endif()

set(KPROTOCOL_GENERATE_VERSIONS "all-known"
    CACHE STRING
    "Comma-separated Minecraft versions to pass to tools/generate_packets.mjs (used by the kprotocol_generate_packets target)")
set(KPROTOCOL_REAL_CLIENT_CMD ""
    CACHE STRING
    "Shell command/template for the opt-in real-client integration test. Placeholders: {host} {port} {version} {wire}")
set(KPROTOCOL_REAL_CLIENT_VERSIONS "1.8:47,1.21.1:767"
    CACHE STRING
    "Comma-separated name:wire entries for the opt-in real-client integration test")
set(KPROTOCOL_REAL_CLIENT_PORT "25565"
    CACHE STRING
    "Local port used by the opt-in real-client integration test")
set(KPROTOCOL_ASIO_SOURCE_DIR ""
    CACHE PATH
    "Path to a local Asio checkout containing asio/include; avoids network fetches when set")

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Do not update already-populated FetchContent dependencies during configure" FORCE)

find_package(ZLIB REQUIRED)
include(GNUInstallDirs)

set(KPROTOCOL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(KPROTOCOL_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(GENERATED_DIR "${KPROTOCOL_SOURCE_DIR}/generated")

function(kprotocol_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(kprotocol_enable_asserts target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

# ============================================================================
# kprotocol_codec: pure wire codec + version table. No packets or registries.
# ============================================================================
add_library(kprotocol_codec
    src/codec.cpp
    src/version.cpp
)
set_target_properties(kprotocol_codec PROPERTIES EXPORT_NAME codec)
target_compile_features(kprotocol_codec PUBLIC cxx_std_20)
target_include_directories(kprotocol_codec PUBLIC
    $<BUILD_INTERFACE:${KPROTOCOL_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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
    src/translation_mappings_loader.cpp
    src/entity_metadata.cpp
    src/blocks.cpp
    src/block_registry.cpp
    src/metadata_registry.cpp
    src/block_registry_data.cpp
    src/metadata_registry_data.cpp
)
set_target_properties(kprotocol_core PROPERTIES EXPORT_NAME core)
target_link_libraries(kprotocol_core PUBLIC kprotocol_codec)
if(EXISTS "${KPROTOCOL_SOURCE_DIR}/data/translation_mappings.bin.gz")
    target_compile_definitions(kprotocol_core PRIVATE
        KPROTOCOL_BLOCK_MAPPINGS_GZ="${KPROTOCOL_SOURCE_DIR}/data/translation_mappings.bin.gz")
endif()
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
set_target_properties(kprotocol_packets PROPERTIES EXPORT_NAME packets)
target_link_libraries(kprotocol_packets PUBLIC kprotocol_core)

# The committed generator output lives under generated/include and
# generated/registry. Header layout: generated/include/kprotocol/generated/*.hpp
# so a single -I generated/include exposes them at #include "kprotocol/generated/...".
if(KPROTOCOL_BUILD_GENERATED_PACKETS
        AND (NOT EXISTS "${GENERATED_DIR}/include"
             OR NOT EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp"))
    message(FATAL_ERROR
        "KPROTOCOL_BUILD_GENERATED_PACKETS=ON requires generated packet files. "
        "Run `npm ci && npm run generate`, or configure with "
        "-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF for the baseline-only build.")
endif()
if(KPROTOCOL_BUILD_GENERATED_PACKETS)
    target_include_directories(kprotocol_packets PUBLIC
        $<BUILD_INTERFACE:${GENERATED_DIR}/include>
    )
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
                ${KPROTOCOL_SOURCE_DIR}/tools/generate_packets.mjs
                --out      ${GENERATED_DIR}
                --versions ${KPROTOCOL_GENERATE_VERSIONS}
        WORKING_DIRECTORY ${KPROTOCOL_SOURCE_DIR}
        COMMENT "Regenerating packet catalog into ${GENERATED_DIR} (versions: ${KPROTOCOL_GENERATE_VERSIONS})"
        VERBATIM
    )
    add_custom_target(kprotocol_embed_translation_mappings
        COMMAND ${KPROTOCOL_NODE_EXECUTABLE}
                ${KPROTOCOL_SOURCE_DIR}/tools/embed_translation_mappings.mjs
        WORKING_DIRECTORY ${KPROTOCOL_SOURCE_DIR}
        COMMENT "Building data/translation_mappings.bin.gz from tools/translation_mappings.json"
        VERBATIM
    )
endif()

# ============================================================================
# Asio (standalone, header-only) — used only by kprotocol_server.
# ============================================================================
include(FetchContent)
if(KPROTOCOL_ASIO_SOURCE_DIR)
    if(NOT EXISTS "${KPROTOCOL_ASIO_SOURCE_DIR}/asio/include")
        message(FATAL_ERROR
            "KPROTOCOL_ASIO_SOURCE_DIR must point to an Asio source tree "
            "containing asio/include")
    endif()
    set(KPROTOCOL_ASIO_INCLUDE_DIR "${KPROTOCOL_ASIO_SOURCE_DIR}/asio/include")
    add_library(kprotocol_asio INTERFACE)
    target_include_directories(kprotocol_asio SYSTEM INTERFACE
        $<BUILD_INTERFACE:${KPROTOCOL_ASIO_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(kprotocol_asio INTERFACE ASIO_STANDALONE)
    if(WIN32)
        target_compile_definitions(kprotocol_asio INTERFACE _WIN32_WINNT=0x0A00)
    endif()
    set_target_properties(kprotocol_asio PROPERTIES EXPORT_NAME asio)
    add_library(kprotocol::asio ALIAS kprotocol_asio)
else()
    if(NOT KPROTOCOL_FETCH_ASIO)
        message(FATAL_ERROR
            "Standalone Asio is required. Set KPROTOCOL_ASIO_SOURCE_DIR to a "
            "local Asio checkout, or enable KPROTOCOL_FETCH_ASIO.")
    endif()
    FetchContent_Declare(
        kprotocol_asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG        asio-1-30-2
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   FALSE
    )
    FetchContent_GetProperties(kprotocol_asio)
    if(NOT kprotocol_asio_POPULATED)
        FetchContent_MakeAvailable(kprotocol_asio)
    endif()
    set(KPROTOCOL_ASIO_INCLUDE_DIR "${kprotocol_asio_SOURCE_DIR}/asio/include")
    if(NOT TARGET kprotocol_asio)
        add_library(kprotocol_asio INTERFACE)
        target_include_directories(kprotocol_asio SYSTEM INTERFACE
            $<BUILD_INTERFACE:${KPROTOCOL_ASIO_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
        target_compile_definitions(kprotocol_asio INTERFACE ASIO_STANDALONE)
        if(WIN32)
            target_compile_definitions(kprotocol_asio INTERFACE _WIN32_WINNT=0x0A00)
        endif()
    endif()
    set_target_properties(kprotocol_asio PROPERTIES EXPORT_NAME asio)
    add_library(kprotocol::asio ALIAS kprotocol_asio)
endif()

# ============================================================================
# kprotocol_server: Asio coroutine TCP runtime + ClientSession.
# ============================================================================
add_library(kprotocol_server
    src/server.cpp
    src/login_security.cpp
    src/configuration.cpp
    src/client_ui.cpp
    src/play_demo.cpp
    src/protocol_server.cpp
)
set_target_properties(kprotocol_server PROPERTIES EXPORT_NAME server)
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
set_target_properties(kprotocol PROPERTIES EXPORT_NAME kprotocol)
target_link_libraries(kprotocol INTERFACE
    kprotocol_codec
    kprotocol_core
    kprotocol_packets
    kprotocol_server
)
add_library(kprotocol::kprotocol ALIAS kprotocol)

# Optional outbound-client functionality. Existing server/codec-only builds do
# not need curl, OpenSSL or JSON. Public targets work in-tree and after install.
set(KPROTOCOL_OPTIONAL_TARGETS "")
if(KPROTOCOL_BUILD_AUTH)
    find_package(CURL REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        find_package(nlohmann_json 3.11 REQUIRED)
    endif()
    add_library(kprotocol_auth src/microsoft_auth.cpp src/auth_cache.cpp src/http.cpp src/crypto.cpp)
    set_target_properties(kprotocol_auth PROPERTIES EXPORT_NAME auth)
    target_compile_features(kprotocol_auth PUBLIC cxx_std_20)
    target_compile_definitions(kprotocol_auth PUBLIC KPROTOCOL_HAS_AUTH=1)
    target_include_directories(kprotocol_auth PUBLIC
        $<BUILD_INTERFACE:${KPROTOCOL_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(kprotocol_auth PUBLIC CURL::libcurl OpenSSL::Crypto
        nlohmann_json::nlohmann_json Threads::Threads)
    if(WIN32)
        target_link_libraries(kprotocol_auth PRIVATE crypt32)
    endif()
    kprotocol_apply_warnings(kprotocol_auth)
    add_library(kprotocol::auth ALIAS kprotocol_auth)
    target_link_libraries(kprotocol INTERFACE kprotocol_auth)
    list(APPEND KPROTOCOL_OPTIONAL_TARGETS kprotocol_auth)
endif()
if(KPROTOCOL_BUILD_CLIENT)
    add_library(kprotocol_client src/minecraft_client.cpp src/chat.cpp src/message_queue.cpp)
    set_target_properties(kprotocol_client PROPERTIES EXPORT_NAME client)
    target_compile_definitions(kprotocol_client PUBLIC KPROTOCOL_HAS_CLIENT=1)
    target_link_libraries(kprotocol_client PUBLIC kprotocol::auth kprotocol::codec kprotocol::asio)
    if(WIN32)
        target_link_libraries(kprotocol_client PRIVATE ws2_32 mswsock)
    endif()
    kprotocol_apply_warnings(kprotocol_client)
    add_library(kprotocol::client ALIAS kprotocol_client)
    target_link_libraries(kprotocol INTERFACE kprotocol_client)
    list(APPEND KPROTOCOL_OPTIONAL_TARGETS kprotocol_client)
endif()

# ============================================================================
# Examples
# ============================================================================
if(KPROTOCOL_BUILD_EXAMPLES OR KPROTOCOL_BUILD_REAL_CLIENT_TESTS)
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

    if(KPROTOCOL_BUILD_AUTH)
        add_executable(kprotocol_auth_tests tests/auth_tests.cpp)
        target_link_libraries(kprotocol_auth_tests PRIVATE kprotocol::auth)
        add_test(NAME kprotocol_auth_tests COMMAND kprotocol_auth_tests)
        set_tests_properties(kprotocol_auth_tests PROPERTIES TIMEOUT 30)
    endif()
    if(KPROTOCOL_BUILD_CLIENT)
        add_executable(kprotocol_client_tests tests/client_tests.cpp)
        target_link_libraries(kprotocol_client_tests PRIVATE kprotocol::client)
        add_test(NAME kprotocol_client_tests COMMAND kprotocol_client_tests)
        set_tests_properties(kprotocol_client_tests PROPERTIES TIMEOUT 30)
    endif()

    set(KPROTOCOL_TEST_NAMES
        codec_tests
        translation_tests
        compression_tests
        integration_tests
        assert_config_tests
        varint_property_tests
        frame_malformed_tests
        version_tests
        version_range_tests
        decompression_bomb_tests
        nbt_tests
        login_security_tests
        field_types_tests
        versioned_schema_tests
        generated_packets_tests
        server_smoke_tests
        server_connection_management_tests
        server_login_tests
        configuration_phase_tests
        gameplay_session_tests
        gameplay_integration_tests
        server_play_demo_tests
        blocks_metadata_tests
        easy_use_tests
        ui_tests
    )

    set(KPROTOCOL_GENERATED_REQUIRED_TESTS
        generated_packets_tests
        version_range_tests
        server_login_tests
        configuration_phase_tests
        gameplay_integration_tests
        server_play_demo_tests
        blocks_metadata_tests
        easy_use_tests
        ui_tests
    )

    set(KPROTOCOL_GENERATED_TESTS_AVAILABLE OFF)
    if(KPROTOCOL_BUILD_GENERATED_PACKETS
            AND EXISTS "${GENERATED_DIR}/registry/register_all_packets.cpp")
        set(KPROTOCOL_GENERATED_TESTS_AVAILABLE ON)
    endif()

    foreach(test_name IN LISTS KPROTOCOL_TEST_NAMES)
        # Tests that reference register_generated_packets need the committed
        # generator output to be compiled in. Skip them if the user opted out.
        if(test_name IN_LIST KPROTOCOL_GENERATED_REQUIRED_TESTS
                AND NOT KPROTOCOL_GENERATED_TESTS_AVAILABLE)
            continue()
        endif()
        set(test_path "${KPROTOCOL_SOURCE_DIR}/tests/${test_name}.cpp")
        if(EXISTS "${test_path}")
            add_executable(kprotocol_${test_name} ${test_path})
            target_link_libraries(kprotocol_${test_name} PRIVATE kprotocol::kprotocol)
            kprotocol_enable_asserts(kprotocol_${test_name})
            add_test(NAME kprotocol_${test_name} COMMAND kprotocol_${test_name})
        endif()
    endforeach()

    if(KPROTOCOL_BUILD_REAL_CLIENT_TESTS)
        if(NOT KPROTOCOL_NODE_EXECUTABLE)
            message(FATAL_ERROR "KPROTOCOL_BUILD_REAL_CLIENT_TESTS requires Node.js")
        endif()
        if(KPROTOCOL_REAL_CLIENT_CMD STREQUAL "")
            message(FATAL_ERROR "KPROTOCOL_BUILD_REAL_CLIENT_TESTS requires -DKPROTOCOL_REAL_CLIENT_CMD=<command>")
        endif()
        add_test(
            NAME kprotocol_real_client_smoke
            COMMAND ${KPROTOCOL_NODE_EXECUTABLE}
                    ${KPROTOCOL_SOURCE_DIR}/tools/real_client_smoke.mjs
                    --server $<TARGET_FILE:kprotocol_play_demo>
                    --client-cmd ${KPROTOCOL_REAL_CLIENT_CMD}
                    --versions ${KPROTOCOL_REAL_CLIENT_VERSIONS}
                    --port ${KPROTOCOL_REAL_CLIENT_PORT}
        )
        set_tests_properties(kprotocol_real_client_smoke PROPERTIES
            LABELS "real-client;integration"
            TIMEOUT 180)
    endif()
endif()

# ============================================================================
# Install / export
# ============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS
        kprotocol kprotocol_codec kprotocol_core kprotocol_packets kprotocol_server kprotocol_asio
        ${KPROTOCOL_OPTIONAL_TARGETS}
    EXPORT kprotocolTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")

install(DIRECTORY "${KPROTOCOL_ASIO_INCLUDE_DIR}/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "*.ipp")

if(EXISTS "${KPROTOCOL_SOURCE_DIR}/data/translation_mappings.bin.gz")
    install(FILES "${KPROTOCOL_SOURCE_DIR}/data/translation_mappings.bin.gz"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/kprotocol)
endif()

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

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/kprotocolConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kprotocol
    PATH_VARS CMAKE_INSTALL_DATADIR
)

if(KPROTOCOL_BUILD_TESTS)
    set(KPROTOCOL_INSTALL_SMOKE_PREFIX
        "${KPROTOCOL_BINARY_DIR}/install-smoke/prefix")
    set(KPROTOCOL_INSTALL_SMOKE_BUILD
        "${KPROTOCOL_BINARY_DIR}/install-smoke/consumer-build")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/install_consumer_smoke.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/install_consumer_smoke.cmake"
        @ONLY)
    add_test(
        NAME kprotocol_install_consumer_smoke
        COMMAND ${CMAKE_COMMAND}
                -DINSTALL_CONFIG=$<CONFIG>
                -P "${CMAKE_CURRENT_BINARY_DIR}/install_consumer_smoke.cmake")
endif()

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/kprotocolConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kprotocol
)

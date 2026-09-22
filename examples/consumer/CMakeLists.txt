cmake_minimum_required(VERSION 3.21)
project(kprotocol_consumer LANGUAGES CXX)

find_package(kprotocol CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE kprotocol::kprotocol)
if(TARGET kprotocol::auth)
    target_sources(consumer PRIVATE auth_consumer.cpp)
endif()
if(TARGET kprotocol::client)
    target_sources(consumer PRIVATE client_consumer.cpp)
endif()

if(kprotocol_HAS_TRANSLATION_MAPPINGS)
    target_compile_definitions(consumer PRIVATE
        KPROTOCOL_CONSUMER_TRANSLATION_MAPPINGS="${kprotocol_TRANSLATION_MAPPINGS}")
endif()

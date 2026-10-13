cmake_minimum_required(VERSION 3.25)

project(veil
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(boost_asio CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_executable(veil
    src/main.cpp
    src/veil/network/tcp_proxy.cpp
    src/veil/network/proxy_session.cpp
    src/veil/network/tls_client.cpp
    src/veil/riot/launcher.cpp
    src/veil/riot/client.cpp
    src/veil/network/http_server.cpp
    src/veil/xmpp/stream_parser.cpp
    src/veil/xmpp/presence_filter.cpp
    src/veil/config/environment.cpp
)

target_include_directories(veil
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(veil
    PRIVATE
        Boost::asio
        Boost::json
        OpenSSL::SSL
        OpenSSL::Crypto
        pugixml::pugixml
)

if(WIN32)
    add_custom_command(TARGET veil POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t
            $<TARGET_FILE_DIR:veil>
            $<TARGET_RUNTIME_DLLS:veil>
        COMMAND_EXPAND_LISTS
    )
endif()

if(MSVC)
    target_compile_options(veil PRIVATE /EHsc)
    target_compile_definitions(veil PRIVATE _WIN32_WINNT=0x0A00)
endif()

if(WIN32)
    target_link_libraries(veil PRIVATE ws2_32)
endif()

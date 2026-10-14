find_package(GTest CONFIG REQUIRED)

add_executable(veil_tests
    xmpp/presence_filter_tests.cpp
    xmpp/stream_parser_tests.cpp

    ${PROJECT_SOURCE_DIR}/src/veil/xmpp/presence_filter.cpp
    ${PROJECT_SOURCE_DIR}/src/veil/xmpp/stream_parser.cpp
)

target_include_directories(veil_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(veil_tests
    PRIVATE
        GTest::gtest_main
        pugixml::pugixml
        spdlog::spdlog
)

include(GoogleTest)
gtest_discover_tests(veil_tests)
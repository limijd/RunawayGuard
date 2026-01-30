cmake_minimum_required(VERSION 3.16)
project(runaway-gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TrayIcon.cpp
    src/DaemonClient.cpp
    src/ProcessTab.cpp
    src/AlertTab.cpp
    src/WhitelistTab.cpp
    src/SettingsTab.cpp
)

set(HEADERS
    src/MainWindow.h
    src/TrayIcon.h
    src/DaemonClient.h
    src/ProcessTab.h
    src/AlertTab.h
    src/WhitelistTab.h
    src/SettingsTab.h
)

add_executable(runaway-gui ${SOURCES} ${HEADERS})
target_link_libraries(runaway-gui PRIVATE Qt6::Widgets Qt6::Network)
target_compile_definitions(runaway-gui PRIVATE QT_DISABLE_DEPRECATED_BEFORE=0x060000)
install(TARGETS runaway-gui DESTINATION bin)

# Unit tests
option(BUILD_TESTING "Build unit tests" OFF)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()

    add_executable(test_daemon_client tests/test_daemon_client.cpp src/DaemonClient.cpp)
    target_link_libraries(test_daemon_client PRIVATE Qt6::Widgets Qt6::Network Qt6::Test)
    target_include_directories(test_daemon_client PRIVATE src)
    add_test(NAME DaemonClientTest COMMAND test_daemon_client)
endif()

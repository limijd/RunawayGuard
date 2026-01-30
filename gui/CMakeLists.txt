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

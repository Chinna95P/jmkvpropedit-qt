cmake_minimum_required(VERSION 3.22)
project(JMkvpropeditQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets)
find_package(KF6WindowSystem QUIET)

add_executable(jmkvpropedit-qt
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/trackeditor.cpp
    src/trackeditor.h
    resources.qrc
)

target_link_libraries(jmkvpropedit-qt PRIVATE Qt6::Widgets)
target_compile_options(jmkvpropedit-qt PRIVATE -Wall -Wextra -Wpedantic)
if(KF6WindowSystem_FOUND)
    target_link_libraries(jmkvpropedit-qt PRIVATE KF6::WindowSystem)
    target_compile_definitions(jmkvpropedit-qt PRIVATE HAVE_KWINDOWSYSTEM)
endif()

install(TARGETS jmkvpropedit-qt RUNTIME DESTINATION bin)
install(FILES packaging/io.github.brunorex.JMkvpropeditQt.desktop
        DESTINATION share/applications)
install(FILES packaging/io.github.brunorex.JMkvpropeditQt.svg
        DESTINATION share/icons/hicolor/scalable/apps)

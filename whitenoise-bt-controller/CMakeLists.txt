cmake_minimum_required(VERSION 2.8.12)

project(whitenoise-bt-controller)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Core)
find_package(Qt5Bluetooth)

add_executable(${PROJECT_NAME} "main.cpp" "bt_server.cpp" "bt_server.h")

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Bluetooth)

install(TARGETS ${PROJECT_NAME}  DESTINATION bin)

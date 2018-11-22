cmake_minimum_required(VERSION 2.8.12)

project(whitenoise-bt-controller)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Core)
find_package(Qt5Bluetooth)
find_package(Qt5Multimedia)
find_package(KF5BluezQt)

add_executable(${PROJECT_NAME} "main.cpp" noise_device.h)

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Bluetooth Qt5::Multimedia KF5::BluezQt)

add_executable(noise-generator-test "noise_generator_test.cpp" noise_device.h)
target_link_libraries(noise-generator-test Qt5::Core Qt5::Multimedia)

install(TARGETS ${PROJECT_NAME}  DESTINATION bin)

cmake_minimum_required(VERSION 2.9)
project(FlipTracker)

set(CMAKE_CXX_STANDARD 11)

find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0)
pkg_check_modules(NOTIFY libnotify)
message(info ${GTKMM_LIBRARY_DIRS} ${NOTIFY_LIBRARY_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS} ${NOTIFY_INCLUDE_DIRS})

set(SOURCE_FILES main.cpp)
add_executable(FlipTracker ${SOURCE_FILES})
target_link_libraries(FlipTracker ${GTKMM_LIBRARIES} ${NOTIFY_LIBRARIES})

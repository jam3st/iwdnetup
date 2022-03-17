cmake_minimum_required(VERSION 3.22)
set(PKG_CONFIG_ARGN "")
set(PROJECT iwdnetup)
project(${PROJECT})

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(DBUSGLIB REQUIRED dbus-glib-1)

set(SOURCE_FILES src/iwdnetup.c)
add_executable(${PROJECT} ${SOURCE_FILES})

target_include_directories(${PROJECT} PUBLIC ${GLIB_INCLUDE_DIRS} ${DBUSGLIB_INCLUDE_DIRS})
target_link_libraries(${PROJECT} ${GLIB_LIBRARIES} ${DBUSGLIB_LIBRARIES})
install(TARGETS ${PROJECT})

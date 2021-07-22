PROJECT(project)
cmake_minimum_required(VERSION 2.6)
SET(CMAKE_BUILD_TYPE "Debug")
#add_definitions(-DNDEBUG)
include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/third_party/)
set(libengine_LIB
${CMAKE_SOURCE_DIR}/egn_core.c
${CMAKE_SOURCE_DIR}/egn_epoll.c
${CMAKE_SOURCE_DIR}/egn_inet.c
${CMAKE_SOURCE_DIR}/egn_util.c
${CMAKE_SOURCE_DIR}/third_party/log.c
${CMAKE_SOURCE_DIR}/third_party/rbtree.c
)
add_library(libengine STATIC ${libengine_LIB})

set(EXECUTABLE_NAME "t_egn")
add_executable(${EXECUTABLE_NAME} ${CMAKE_SOURCE_DIR}/test/egn_test.cc)
target_link_libraries(${EXECUTABLE_NAME} libengine)

set(EXECUTABLE_NAME "t_timer")
add_executable(${EXECUTABLE_NAME} ${CMAKE_SOURCE_DIR}/test/timer_test.cc)
target_link_libraries(${EXECUTABLE_NAME} libengine)

set(EXECUTABLE_NAME "tcp_echo_s")
add_executable(${EXECUTABLE_NAME} ${CMAKE_SOURCE_DIR}/test/tcp_echo_server.cc)
target_link_libraries(${EXECUTABLE_NAME} libengine)

set(EXECUTABLE_NAME "tcp_echo_c")
add_executable(${EXECUTABLE_NAME} ${CMAKE_SOURCE_DIR}/test/tcp_echo_client.c)


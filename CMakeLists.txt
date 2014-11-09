cmake_minimum_required(VERSION 2.8)

project(with_smaa)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR})

find_package(DL REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_OPT} -Wall -Wextra -O2 -std=c99")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_executable(
  with_smaa
  src/with_smaa.c
  )

add_library(
  with_smaa_shim
  SHARED
  src/shim.c
  src/smaa.c
  )
  

install (
  TARGETS with_smaa
  RUNTIME DESTINATION bin
  )


cmake_minimum_required(VERSION 3.2)

project(lps)
find_package(Git)
execute_process(COMMAND
  "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SHA1
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the date of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_DATE
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the subject of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%s
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# generate version.cc
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.cc.in" "${CMAKE_CURRENT_BINARY_DIR}/src/version.cc" @ONLY)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/.install/)


set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/tests/ ${PROJECT_SOURCE_DIR}/src/)

include_directories(${INCLUDE_DIR})

include(ExternalProject)

ExternalProject_Add(verilator
  GIT_REPOSITORY https://github.com/verilator/verilator.git
  GIT_TAG        v5.016
  PREFIX "${PROJECT_SOURCE_DIR}/.tmp/"
  CONFIGURE_COMMAND cd ${PROJECT_SOURCE_DIR}/.tmp/src/verilator && autoconf &&  ./configure --prefix=${CMAKE_INSTALL_PREFIX}
  BUILD_COMMAND cd ${PROJECT_SOURCE_DIR}/.tmp/src/verilator && make -j6
  INSTALL_COMMAND  cd ${PROJECT_SOURCE_DIR}/.tmp/src/verilator && make install
)




add_subdirectory(src)
# add_subdirectory(tests)

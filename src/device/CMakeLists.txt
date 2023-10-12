find_package(verilator HINTS ${PROJECT_SOURCE_DIR}/.tmp/src/verilator/)

if (NOT verilator_FOUND)
    message(FATAL_ERROR "Verilator was not found. ")
endif()

set(RTL_DIR ${CMAKE_SOURCE_DIR}/rtl)
set(TOP GPGPU_top)

file(GLOB RTL_SRC "${RTL_DIR}/${TOP}.v")

set(CXX_FLAGS -std=c++11 -Wall -Wextra -Wfatal-errors -Wno-array-bounds-Wno-maybe-uninitialized)

set(VL_FLAGS --cc ${TOP} --top-module ${TOP} -O2 --language 1800-2009 --assert -Wall -Wpedantic
            -Wno-DECLFILENAME -Wno-REDEFMACRO -Wno-MODDUP -Wno-UNUSEDSIGNAL
            --x-initial unique --x-assign unique
            --debug --trace-structs
            )
            
add_library(rtllib SHARED)
verilate(rtllib
        TRACE
        SOURCES ${RTL_SRC}
        TOP_MODULE ${TOP}
        INCLUDE_DIRS ${RTL_DIR}
        THREADS ${threads_var}
        OPT_GLOBAL ${CXX_FLAGS}
        VERILATOR_ARGS  ${VL_FLAGS}
        )

file(GLOB_RECURSE SRCS "*.cc")

add_library(rtlsim SHARED ${SRCS})

target_link_libraries(rtlsim PUBLIC rtllib)



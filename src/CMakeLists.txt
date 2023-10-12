add_subdirectory(device)

file(GLOB_RECURSE SRCS "./*.cc")

add_library(rtldriver SHARED ${SRCS})
add_dependencies(rtldriver rtlsim)
target_link_libraries(rtldriver PUBLIC rtlsim)

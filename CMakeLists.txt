cmake_minimum_required(VERSION 3.16)

project(lzdatagen C)

include(CheckLibraryExists)

# Check if we need to link with math library
check_library_exists(m pow "" LZDG_HAVE_M)

#
# lzdatagen
#
add_library(lzdatagen lzdatagen.c pcg_basic.c)
target_include_directories(lzdatagen PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)
target_link_libraries(lzdatagen PUBLIC $<$<BOOL:${LZDG_HAVE_M}>:m>)

add_library(lzdatagen::lzdatagen ALIAS lzdatagen)

#
# lzdgen
#
add_executable(lzdgen lzdgen.c parg.c)
target_link_libraries(lzdgen PRIVATE lzdatagen::lzdatagen)

add_executable(lzdatagen::lzdgen ALIAS lzdgen)

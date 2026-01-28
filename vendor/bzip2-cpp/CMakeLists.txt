cmake_minimum_required(VERSION 3.14)

project(
	bzip2-cpp-lib 
	VERSION 1.0
	DESCRIPTION "A simple C++ wrapper for the bzip2 library"
)

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
	-fno-omit-frame-pointer \
	-Wpedantic \
	-Wall \
	-Wextra \
	-Weffc++ \
	-Wdisabled-optimization \
	-Wfloat-equal \
	-Wno-unused-variable \
	-Wswitch-enum \
	-Wno-switch-enum"
)

# bzip2
add_library(bzip2 STATIC
  bzip2/blocksort.c
  bzip2/bzlib.c
  bzip2/compress.c
  bzip2/crctable.c
  bzip2/decompress.c
  bzip2/huffman.c
  bzip2/randtable.c
)
target_include_directories(bzip2 PUBLIC bzip2)

# bzip2-cpp-lib
add_library(bzip2-cpp-lib STATIC)
target_sources(bzip2-cpp-lib PRIVATE
	src/bzip2-cpp-lib.cpp
)
target_include_directories(bzip2-cpp-lib PUBLIC include)
target_link_libraries(bzip2-cpp-lib PRIVATE bzip2)

# test executable
add_executable(bzip2-cpp-lib-test
	test/test.cpp
)
target_link_libraries(bzip2-cpp-lib-test PRIVATE bzip2-cpp-lib)

# copy test files directory
file(COPY test/test_files DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


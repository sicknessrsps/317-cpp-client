#https://github.com/Ravbug/sdl3-sample/blob/main/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

option(PACK_CACHE "Pack cache" ON)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories.
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

# Declare the project
project(SDL-Client)

if(EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")    # Disable shared builds on platforms where it does not make sense to use them
    set(SDL_SHARED OFF)
endif()

if(MSVC)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        add_definitions(/MP)				# parallelize each target, unless Ninja is the generator
    endif()
endif()

# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

# on Web targets, we need CMake to generate a HTML webpage.
if(EMSCRIPTEN)
    add_compile_options(-pthread)
    add_link_options(-pthread -sUSE_PTHREADS=1)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

add_subdirectory(vendor)
add_subdirectory(src)

if(EMSCRIPTEN)
    target_link_options(${EXECUTABLE_NAME} PRIVATE
            #"-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=256MB"
            "-sUSE_PTHREADS=1"
            "-sPTHREAD_POOL_SIZE=6"
            "-pthread"
            "-sASYNCIFY"
            "-sASYNCIFY_STACK_SIZE=65536"
            "-sOFFSCREEN_FRAMEBUFFER=1"
    )
    # Release optimizations for Emscripten
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${EXECUTABLE_NAME} PRIVATE -O3)
        target_link_options(${EXECUTABLE_NAME} PRIVATE -O3)
    endif()
endif()

# Dealing with assets
# We have some non-code resources that our application needs in order to work. How we deal with those differs per platform.
if(EMSCRIPTEN AND PACK_CACHE)
    # on the web, we have to put the files inside of the webassembly
    # somewhat unintuitively, this is done via a linker argument.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/main_file_cache.dat@/cache/"
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/main_file_cache.idx0@/cache/"
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/main_file_cache.idx1@/cache/"
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/main_file_cache.idx2@/cache/"
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/main_file_cache.idx3@/cache/"
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/main_file_cache.idx4@/cache/"
        "--preload-file=${CMAKE_CURRENT_LIST_DIR}/cache/Old_School_RuneScape.sf2@/cache/"
    )
endif()

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")
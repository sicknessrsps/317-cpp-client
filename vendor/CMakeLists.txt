set(SDL_TEST OFF)

if(EMSCRIPTEN)
    set(SDL_PTHREADS ON CACHE BOOL "Use POSIX threads for multi-threading" FORCE)
    set(SDL_THREADS ON CACHE BOOL "Enable the thread subsystem" FORCE)
endif()

set(SDLTTF_VENDORED ON) # tell SDL_ttf to build its own dependencies

# SDL_image (used for loading various image formats)
set(SDLIMAGE_VENDORED OFF)
set(SDLIMAGE_AVIF OFF)	# disable formats we don't use to make the build faster and smaller.
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_JPEG OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_GIF OFF)

set(ZLIB_BUILD_TESTING OFF)
set(ZLIB_BUILD_SHARED ON)
set(ZLIB_BUILD_STATIC OFF)

add_subdirectory(SDL)
add_subdirectory(SDL_image EXCLUDE_FROM_ALL)
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)
add_subdirectory(SDL_net EXCLUDE_FROM_ALL)

add_subdirectory(bzip2-cpp)
add_subdirectory(zlib)

# TinySoundFont - header-only MIDI synthesizer
add_library(TinySoundFont INTERFACE)
target_include_directories(TinySoundFont INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/TinySoundFont)

add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE
    SDL3_ttf::SDL3_ttf
    SDL3_image::SDL3_image
    SDL3_net::SDL3_net
    SDL3::SDL3
    bzip2
    zlib
    TinySoundFont
)
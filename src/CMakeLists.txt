file(GLOB_RECURSE CORE_SRC "*.cpp" "*.h")

# Static library for shared code
add_library(SDL_ClientLib STATIC ${CORE_SRC})
target_link_libraries(SDL_ClientLib PRIVATE vendor)

# Main executable
add_executable(SDL-Client Main.cpp)   # only main.cpp here
target_precompile_headers(SDL-Client PRIVATE "PCH.h")
target_link_libraries(SDL-Client PRIVATE SDL_ClientLib vendor)

# Copy cache directory to output directory for non-Emscripten builds
if(NOT EMSCRIPTEN)
    add_custom_command(TARGET SDL-Client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_LIST_DIR}/../cache"
            "$<TARGET_FILE_DIR:SDL-Client>/cache"
        COMMENT "Copying cache directory to output directory"
    )
endif()
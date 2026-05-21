cmake_minimum_required(VERSION 3.20)
project(autoball C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find SDL3
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_shadercross CONFIG REQUIRED)

# Collect sources
file(GLOB_RECURSE AUTOBALL_SOURCES
    src/*.c
)

add_executable(autoball ${AUTOBALL_SOURCES})

target_include_directories(autoball PRIVATE
    include
    src
)

target_link_libraries(autoball PRIVATE
    SDL3::SDL3
    SDL3_shadercross::SDL3_shadercross
)

# Copy assets to build directory
add_custom_command(TARGET autoball POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:autoball>/assets
    COMMENT "Copying assets..."
)

# Copy shaders to build directory
add_custom_command(TARGET autoball POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:autoball>/shaders
    COMMENT "Copying shaders..."
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(autoball PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Copy SDL3.dll on Windows
    add_custom_command(TARGET autoball POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:autoball>
        COMMENT "Copying SDL3.dll..."
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(autoball PRIVATE /W4)
else()
    target_compile_options(autoball PRIVATE -Wall -Wextra -Wpedantic)
endif()



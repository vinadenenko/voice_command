# Basic Example Application
cmake_minimum_required(VERSION 3.20)
project(basic_example)


# if (WHISPER_SDL2)
    # SDL2
    find_package(SDL2 REQUIRED)

    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)

    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
    message(STATUS "SDL2_LIBRARIES    = ${SDL2_LIBRARIES}")
# endif()

set(TARGET common)

add_library(${TARGET} STATIC
    common.h
    common.cpp
    common-ggml.h
    common-ggml.cpp
    common-whisper.h
    common-whisper.cpp
    grammar-parser.h
    grammar-parser.cpp
    ${COMMON_SOURCES_FFMPEG}
)

target_link_libraries(${TARGET} PRIVATE whisper-cpp::whisper-cpp ${COMMON_EXTRA_LIBS} ${CMAKE_DL_LIBS})
set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${TARGET} PROPERTIES FOLDER "libs")

# if (WHISPER_SDL2)
    # common-sdl

    set(TARGET common-sdl)

    add_library(${TARGET} STATIC
        common-sdl.h
        common-sdl.cpp
    )

    # include(DefaultTargetOptions)

    target_include_directories(${TARGET} PUBLIC  ${SDL2_INCLUDE_DIRS})
    target_link_libraries     (${TARGET} PRIVATE ${SDL2_LIBRARIES})

    set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(${TARGET} PROPERTIES FOLDER "libs")
# endif()


# Find Voice Command library (if not built in same tree)
if(NOT TARGET voice_command)
    find_package(VoiceCommand REQUIRED)
endif()

# Create example executable
add_executable(basic_example
    basic_example.cpp
)

target_link_libraries(basic_example
    PRIVATE
        voice_command
        common
        common-sdl
)

# Add to examples group
set_target_properties(basic_example PROPERTIES FOLDER "Examples")

# Integration Example - uses voice_command library as a black box (Qt-based)
find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(integration_example
    integration_example.cpp
)

target_link_libraries(integration_example
    PRIVATE
        voice_command
        Qt6::Core
)

set_target_properties(integration_example PROPERTIES
    FOLDER "Examples"
    AUTOMOC ON
)

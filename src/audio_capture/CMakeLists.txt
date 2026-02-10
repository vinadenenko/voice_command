# audio_capture library CMake configuration

# Build options for audio backends
option(VOICE_COMMAND_AUDIO_SDL "Build SDL2 audio capture backend" OFF)
option(VOICE_COMMAND_AUDIO_QT "Build Qt audio capture backend" ON)

# Core source files (always included)
set(AUDIO_CAPTURE_SOURCES
    vad/simple_vad.cpp
)

set(AUDIO_CAPTURE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/audio_capture/audio_capture_config.h
    ${CMAKE_SOURCE_DIR}/include/audio_capture/iaudio_capture.h
    ${CMAKE_SOURCE_DIR}/include/audio_capture/ivad.h
    ${CMAKE_SOURCE_DIR}/include/audio_capture/vad/simple_vad.h
)

# SDL2 backend
if(VOICE_COMMAND_AUDIO_SDL)
    find_package(SDL2 REQUIRED)

    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)

    message(STATUS "audio_capture: SDL2 backend enabled")
    message(STATUS "audio_capture: SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
    message(STATUS "audio_capture: SDL2_LIBRARIES = ${SDL2_LIBRARIES}")

    list(APPEND AUDIO_CAPTURE_SOURCES
        sdl/sdl_audio_capture.cpp
    )
    list(APPEND AUDIO_CAPTURE_HEADERS
        ${CMAKE_SOURCE_DIR}/include/audio_capture/sdl/sdl_audio_capture.h
    )
endif()

# Qt backend
if(VOICE_COMMAND_AUDIO_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)

    message(STATUS "audio_capture: Qt6 backend enabled")
    message(STATUS "audio_capture: Qt6_VERSION = ${Qt6_VERSION}")

    list(APPEND AUDIO_CAPTURE_SOURCES
        qt/qt_audio_capture.cpp
    )
    list(APPEND AUDIO_CAPTURE_HEADERS
        ${CMAKE_SOURCE_DIR}/include/audio_capture/qt/qt_audio_capture.h
    )
endif()

# Create the library
add_library(audio_capture STATIC
    ${AUDIO_CAPTURE_SOURCES}
    ${AUDIO_CAPTURE_HEADERS}
)

# Include directories
target_include_directories(audio_capture
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL2 if enabled
if(VOICE_COMMAND_AUDIO_SDL)
    target_include_directories(audio_capture PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(audio_capture PRIVATE ${SDL2_LIBRARIES})
    target_compile_definitions(audio_capture PUBLIC VOICE_COMMAND_AUDIO_SDL_ENABLED)
endif()

# Link Qt if enabled
if(VOICE_COMMAND_AUDIO_QT)
    # Enable AUTOMOC for Qt's moc processing (required for Q_OBJECT in cpp files)
    set_target_properties(audio_capture PROPERTIES AUTOMOC ON)

    target_link_libraries(audio_capture PRIVATE Qt6::Core Qt6::Multimedia)
    target_compile_definitions(audio_capture PUBLIC VOICE_COMMAND_AUDIO_QT_ENABLED)
endif()

# Library properties
set_target_properties(audio_capture PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    FOLDER "libs"
)

# Compile definitions
target_compile_definitions(audio_capture
    PRIVATE
        VOICE_COMMAND_AUDIO_CAPTURE_BUILDING
)

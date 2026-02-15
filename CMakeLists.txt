cmake_minimum_required(VERSION 3.20)

# C++ standard and project options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(VoiceCommand
    VERSION 0.1.0
    DESCRIPTION "no desc"
    LANGUAGES CXX
)

# Build options
option(VOICE_COMMAND_BUILD_TESTS "Build tests" OFF)
option(VOICE_COMMAND_BUILD_EXAMPLES "Build examples" OFF)
#option(VOICE_COMMAND_BUILD_DOCS "Generate documentation" OFF)
#option(VOICE_COMMAND_INSTALL "Generate install target" ON)


# list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
# list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

# if (CONAN_EXPORTED)
# else ()
#     if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
#         message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
#         file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
#                 "${CMAKE_BINARY_DIR}/conan.cmake"
#                 TLS_VERIFY ON)
#     endif ()

#     include(${CMAKE_BINARY_DIR}/conan.cmake)

#     conan_cmake_autodetect(settings)
#     conan_cmake_install(PATH_OR_REFERENCE ${PROJECT_SOURCE_DIR}/conanfile.py
#             OUTPUT_FOLDER ${CMAKE_BINARY_DIR} BUILD missing SETTINGS ${settings})
# endif ()
# include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)

# include_directories(${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/src)

# Debug/Release configurations
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     add_compile_definitions(VOICE_COMMAND_DEBUG)
#     if(VOICE_COMMAND_ENABLE_OPENGL_DEBUG)
#         add_compile_definitions(VOICE_COMMAND_OPENGL_DEBUG)
#     endif()
# endif()
find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)
# Find required packages
find_package(whisper-cpp REQUIRED)
find_package(httplib REQUIRED)
find_package(nlohmann_json REQUIRED)

# Optional packages for testing and examples
if(VOICE_COMMAND_BUILD_TESTS)
    find_package(GTest REQUIRED)
    find_package(benchmark REQUIRED)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Audio capture library (must be before voice_command since it depends on it)
# add_subdirectory(src/audio_capture)

# Source files
set(VOICE_COMMAND_SOURCES
    # Command framework
    src/command/dispatcher/commanddispatcher.cpp
    src/command/context/commandcontext.cpp
    src/command/descriptor/commanddescriptor.cpp
    src/command/registry/commandregistry.cpp
    src/command/nlu/llm_nlu_engine.cpp
    src/command/nlu/rule_based_nlu_engine.cpp
    src/command/nlu/remote_llm_nlu_engine.cpp
    # Testing utilities
    src/testing/command_tester.cpp
    # Engine wrappers
    src/audio_engine.cpp
    src/local_whisper_engine.cpp
    src/remote_whisper_engine.cpp
    # Recognition strategies
    src/guided_recognition_strategy.cpp
    src/nlu_recognition_strategy.cpp
    # Orchestration
    src/voice_assistant.cpp
    src/qt_voice_assistant.cpp
)

set(VOICE_COMMAND_HEADERS
    # Command framework
    include/command/command_result.h
    include/command/icommand.h
    include/command/dispatcher/commanddispatcher.h
    include/command/context/commandcontext.h
    include/command/descriptor/commanddescriptor.h
    include/command/registry/commandregistry.h
    include/command/nlu/inlu_engine.h
    include/command/nlu/llm_nlu_engine.h
    include/command/nlu/rule_based_nlu_engine.h
    include/command/nlu/remote_llm_nlu_engine.h
    # Testing utilities
    include/testing/command_tester.h
    # Engine wrappers
    include/audio_engine.h
    include/asr_engine.h
    include/local_whisper_engine.h
    include/remote_whisper_engine.h
    # Recognition strategies
    include/recognition_strategy.h
    # Orchestration
    include/voice_assistant.h
    include/qt_voice_assistant.h
)

set(LIBS_TO_LINK
    whisper-cpp::whisper-cpp
    httplib::httplib
    nlohmann_json::nlohmann_json
    Qt6::Core
    Qt6::Multimedia
)

# Build options for audio backends
option(VOICE_COMMAND_AUDIO_SDL "Build SDL2 audio capture backend" OFF)
option(VOICE_COMMAND_AUDIO_QT "Build Qt audio capture backend" ON)

# Core source files (always included)
set(AUDIO_CAPTURE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/audio_capture/vad/simple_vad.cpp
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
        ${CMAKE_SOURCE_DIR}/src/audio_capture/sdl/sdl_audio_capture.cpp
    )
    list(APPEND AUDIO_CAPTURE_HEADERS
        ${CMAKE_SOURCE_DIR}/include/audio_capture/sdl/sdl_audio_capture.h
    )
    # target_link_libraries(voice_command PRIVATE ${SDL2_LIBRARIES})
    list(APPEND LIBS_TO_LINK ${SDL2_LIBRARIES})
endif()

# Qt backend
if(VOICE_COMMAND_AUDIO_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)

    message(STATUS "audio_capture: Qt6 backend enabled")
    message(STATUS "audio_capture: Qt6_VERSION = ${Qt6_VERSION}")

    list(APPEND AUDIO_CAPTURE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/audio_capture/qt/qt_audio_capture.cpp
    )
    list(APPEND AUDIO_CAPTURE_HEADERS
        ${CMAKE_SOURCE_DIR}/include/audio_capture/qt/qt_audio_capture.h
    )

    # target_link_libraries(voice_command PRIVATE Qt6::Core Qt6::Multimedia)
    list(APPEND LIBS_TO_LINK Qt6::Core Qt6::Multimedia)

endif()

# Create the main library
add_library(voice_command SHARED
    ${VOICE_COMMAND_SOURCES} ${VOICE_COMMAND_HEADERS}
    ${AUDIO_CAPTURE_SOURCES} ${AUDIO_CAPTURE_HEADERS}
)

if(WIN32)
set_target_properties(voice_command PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
endif()

if(VOICE_COMMAND_AUDIO_SDL)
    target_include_directories(voice_command PUBLIC ${SDL2_INCLUDE_DIRS})
    target_compile_definitions(voice_command PUBLIC VOICE_COMMAND_AUDIO_SDL_ENABLED)
endif()

if(VOICE_COMMAND_AUDIO_QT)
    # Enable AUTOMOC for Qt's moc processing (required for Q_OBJECT in cpp files)
    set_target_properties(voice_command PROPERTIES AUTOMOC ON)
    target_compile_definitions(voice_command PUBLIC VOICE_COMMAND_AUDIO_QT_ENABLED)
endif()

# Set library properties
set_target_properties(voice_command PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${VOICE_COMMAND_HEADERS}"
    # CXX_VISIBILITY_PRESET default
    LINKER_LANGUAGE CXX
)
# set_target_properties(hello PROPERTIES LINKER_LANGUAGE CXX)
# Link libraries
target_link_libraries(voice_command
    PUBLIC ${LIBS_TO_LINK}

)
set_target_properties(voice_command PROPERTIES
    AUTOMOC ON
)
# Include directories for the library
target_include_directories(voice_command
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile definitions
target_compile_definitions(voice_command
    PRIVATE
        VOICE_COMMAND_BUILDING
    PUBLIC
        VOICE_COMMAND_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        VOICE_COMMAND_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        VOICE_COMMAND_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        VOICE_COMMAND_VERSION="${PROJECT_VERSION}"
)


# Tests
# if(VOICE_COMMAND_BUILD_TESTS)
#     enable_testing()
    
#     # Test sources
#     file(GLOB_RECURSE TEST_SOURCES "tests/unit/*.cpp" "tests/integration/*.cpp" "tests/data/*.cpp")

#     # Create test executable
#     add_executable(voice_command_tests ${TEST_SOURCES})
    
#     target_link_libraries(voice_command_tests
#         PRIVATE
#             voice_command
#             GTest::gtest
#             GTest::gtest_main
#             benchmark::benchmark
#     )
    
#     target_include_directories(voice_command_tests PRIVATE tests)
    
#     # Add test to CTest
#     add_test(NAME UnitTests COMMAND voice_command_tests)
    
#     # Benchmark test
#     if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/performance")
#         file(GLOB_RECURSE PERF_SOURCES "tests/performance/*.cpp")
#         if(PERF_SOURCES)
#             add_executable(voice_command_benchmarks ${PERF_SOURCES})
#             target_link_libraries(voice_command_benchmarks
#                 PRIVATE
#                     voice_command
#                     benchmark::benchmark
#             )
#             target_include_directories(voice_command_benchmarks PRIVATE tests)
#         endif()
#     endif()
# endif()

# Examples
if(VOICE_COMMAND_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# # Documentation
# if(VOICE_COMMAND_BUILD_DOCS)
#     find_package(Doxygen QUIET)
#     if(DOXYGEN_FOUND)
#         set(DOXYGEN_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
#         set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
        
#         configure_file(
#             ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
#             ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
#             @ONLY
#         )
        
#         add_custom_target(docs
#             ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
#             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#             COMMENT "Generating API documentation with Doxygen"
#             VERBATIM
#         )
#     endif()
# endif()

# # Installation
# if(VOICE_COMMAND_INSTALL)
#     include(GNUInstallDirs)
    
#     install(TARGETS voice_command
#         EXPORT VoiceCommandTargets
#         LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#         PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/voice_command
#     )
    
#     install(DIRECTORY include/voice_command
#         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#         FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
#     )
    
#     # Export the targets
#     install(EXPORT VoiceCommandTargets
#         FILE VoiceCommandTargets.cmake
#         NAMESPACE voice_command::
#         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VoiceCommand
#     )
    
#     # Generate config files
#     include(CMakePackageConfigHelpers)
    
#     configure_package_config_file(
#         "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VoiceCommandConfig.cmake.in"
#         "${CMAKE_CURRENT_BINARY_DIR}/VoiceCommandConfig.cmake"
#         INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VoiceCommand
#     )
    
#     write_basic_package_version_file(
#         "${CMAKE_CURRENT_BINARY_DIR}/VoiceCommandConfigVersion.cmake"
#         VERSION ${PROJECT_VERSION}
#         COMPATIBILITY SameMajorVersion
#     )
    
#     install(FILES
#         "${CMAKE_CURRENT_BINARY_DIR}/VoiceCommandConfig.cmake"
#         "${CMAKE_CURRENT_BINARY_DIR}/VoiceCommandConfigVersion.cmake"
#         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VoiceCommand
#     )
# endif()

# # CPack configuration for packaging
# include(CPack)
# set(CPACK_PACKAGE_NAME "VoiceCommand")
# set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
# set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
# set(CPACK_PACKAGE_VENDOR "VoiceCommand Team")
# set(CPACK_PACKAGE_CONTACT "VoiceCommand@example.com")

# if(WIN32)
#     set(CPACK_GENERATOR "NSIS;ZIP")
# elseif(APPLE)
#     set(CPACK_GENERATOR "DragNDrop;TGZ")
# else()
#     set(CPACK_GENERATOR "TGZ;DEB;RPM")
# endif()

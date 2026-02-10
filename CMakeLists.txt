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
#option(VOICE_COMMAND_BUILD_TESTS "Build unit tests" ON)
option(VOICE_COMMAND_BUILD_EXAMPLES "Build examples" ON)
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

# Find required packages
find_package(whisper-cpp REQUIRED)
# find_package(nlohmann_json REQUIRED)
# find_package(spdlog REQUIRED)

# Optional packages for testing and examples
if(VOICE_COMMAND_BUILD_TESTS)
    find_package(GTest REQUIRED)
    find_package(benchmark REQUIRED)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(VOICE_COMMAND_SOURCES
    src/command/dispatcher/commanddispatcher.cpp
    src/command/context/commandcontext.cpp
    src/command/descriptor/commanddescriptor.cpp
    src/command/registry/commandregistry.cpp
    src/command/nlu/llm_nlu_engine.cpp
    src/command/nlu/rule_based_nlu_engine.cpp
)

set(VOICE_COMMAND_HEADERS
    include/command/command_result.h
    include/command/icommand.h
    include/command/dispatcher/commanddispatcher.h
    include/command/context/commandcontext.h
    include/command/descriptor/commanddescriptor.h
    include/command/registry/commandregistry.h
    include/command/nlu/inlu_engine.h
    include/command/nlu/llm_nlu_engine.h
    include/command/nlu/rule_based_nlu_engine.h
)

# Create the main library
add_library(voice_command
    ${VOICE_COMMAND_SOURCES} ${VOICE_COMMAND_HEADERS}
)

# Set library properties
set_target_properties(voice_command PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${VOICE_COMMAND_HEADERS}"
    CXX_VISIBILITY_PRESET hidden
    LINKER_LANGUAGE CXX
)
# set_target_properties(hello PROPERTIES LINKER_LANGUAGE CXX)
# Link libraries
target_link_libraries(voice_command
    PUBLIC
        whisper-cpp::whisper-cpp
        # nlohmann_json::nlohmann_json
        # spdlog::spdlog
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

# Audio capture library
add_subdirectory(src/audio_capture)

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

# Basic Example Application
cmake_minimum_required(VERSION 3.20)
project(basic_example)

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
)

# Add to examples group
set_target_properties(basic_example PROPERTIES FOLDER "Examples")

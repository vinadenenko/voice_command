# Real Application Example

This is an **isolated example application** that demonstrates how to integrate the `voice_command` library as an external dependency, exactly as a real user would.

## Key Points

- **Not part of the main CMake build** - This directory is intentionally excluded from the library's build system
- **Uses voice_command via Conan** - Depends on the library as an external package, not as a subdirectory
- **Realistic integration test** - Validates that the library works correctly when consumed by external projects

## Building

```bash
# 1. First, export the voice_command library to local Conan cache
cd /path/to/voice_command
conan create . --build=missing --version=<version>

# 2. Then build this example as a standalone project in IDE/terminal

```

## What It Demonstrates

- Qt/QML application with a colored rectangle
- Voice commands to change the rectangle color
- Full integration: audio capture, speech recognition, command dispatch

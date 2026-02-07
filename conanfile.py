from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class VoiceCommandConan(ConanFile):
    name = "VoiceCommand"
    version = "0.1.0"
    description = "Desc"
    author = "some team"
    url = "https://github.com/no_url"
    license = "MIT"
    package_type = "library"

    # Package configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
        "with_examples": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": True,
        "with_examples": True
    }

    # Export sources for conan center
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*", "examples/*"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        self.options["whisper-cpp"].shared=True
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        """Core dependencies for library"""

        # Whisper-cpp
        self.requires("whisper-cpp/1.8.2")

        # JSON parsing for configuration and data formats
        # self.requires("nlohmann_json/3.11.2")

        # Logging framework
        self.requires("spdlog/1.13.0")

        # Testing framework (when tests are enabled)
        if self.options.with_tests:
            self.requires("gtest/1.14.0")
            self.requires("benchmark/1.8.3")

    def build_requirements(self):
        """Build-time requirements"""
        self.tool_requires("cmake/[>=3.20]")

    def generate(self):
        """Generate CMake toolchain and dependencies"""
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        #tc.variables["VoiceCommand_BUILD_TESTS"] = self.options.with_tests
        #tc.variables["VoiceCommand_BUILD_EXAMPLES"] = self.options.with_examples
        tc.generate()

    def build(self):
        """Build the project"""
        from conan.tools.cmake import CMake, cmake_program
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        if self.options.with_tests:
            cmake.test()

    def package(self):
        """Package the library"""
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))

        if self.options.shared:
            copy(self, "*.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
            copy(self, "*.dylib*", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            copy(self, "*.so*", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        else:
            copy(self, "*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            copy(self, "*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)

    def package_info(self):
        """Provide package information to consumers"""
        self.cpp_info.libs = ["voice_command"]

        # if self.settings.os in ["Linux", "FreeBSD"]:
        #     self.cpp_info.system_libs.extend(["GL", "X11", "Xrandr", "Xinerama", "Xi", "Xcursor", "dl", "pthread"])
        # elif self.settings.os == "Windows":
        #     self.cpp_info.system_libs.extend(["opengl32", "gdi32", "user32", "kernel32", "shell32"])
        # elif self.settings.os == "Macos":
        #     self.cpp_info.frameworks.extend(["OpenGL", "Cocoa", "IOKit", "CoreVideo"])

        # Define targets for proper transitive dependencies
        self.cpp_info.set_property("cmake_target_name", "voice_command::voice_command")

        # Include directories
        self.cpp_info.includedirs = ["include"]

        # Build type specific flags
        if self.settings.build_type == "Debug":
            self.cpp_info.defines.append("VOICE_COMMAND_DEBUG")

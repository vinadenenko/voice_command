from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class MyApplication(ConanFile):
    name = "my_application"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    # Define dependencies
    def requirements(self):
        self.requires("voice_command/0.1.0")
        # self.requires("nlohmann_json/3.12.0")

    # Define build tools
    #def build_requirements(self):
        #self.tool_requires("cmake/3.26.4")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

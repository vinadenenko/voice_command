import os
import textwrap

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import is_apple_os
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rm, save
from conan.tools.scm import Version

required_conan_version = ">=2.1"


class WhisperCppConan(ConanFile):
    name = "whisper-cpp"
    version = "vulkan"  # Update as needed for local testing
    description = "High-performance inference of OpenAI's Whisper automatic speech recognition (ASR) model"
    topics = ("whisper", "asr")
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/ggerganov/whisper.cpp"
    license = "MIT"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "sanitize_thread": [True, False],
        "sanitize_address": [True, False],
        "sanitize_undefined": [True, False],
        "no_avx": [True, False],
        "no_avx2": [True, False],
        "no_fma": [True, False],
        "no_f16c": [True, False],
        "no_accelerate": [True, False],
        "metal": [True, False],
        "metal_ndebug": [True, False],
        "with_coreml": [True, False],
        "coreml_allow_fallback": [True, False],
        "with_blas": [True, False],
        "with_openvino": [True, False],
        "with_cuda": [True, False],
        "with_vulkan": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "sanitize_thread": False,
        "sanitize_address": False,
        "sanitize_undefined": False,
        "no_avx": False,
        "no_avx2": False,
        "no_fma": False,
        "no_f16c": False,
        "no_accelerate": False,
        "metal": True,
        "metal_ndebug": False,
        "with_coreml": False,
        "coreml_allow_fallback": False,
        "with_blas": False,
        "with_openvino": False,
        "with_cuda": False,
        "with_vulkan": False,
    }
    package_type = "library"
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "ggml/*", "cmake/*", "bindings/*", "LICENSE"

    @property
    def _min_cppstd(self):
        return "14"

    @property
    def _compilers_minimum_version(self):
        return {
            "14": {
                "gcc": "9",
                "clang": "5",
                "apple-clang": "10",
                "Visual Studio": "15",
                "msvc": "191",
            },
        }.get(self._min_cppstd, {})

    @property
    def _cuda_build_module(self):
        return textwrap.dedent("""\
            find_dependency(CUDAToolkit REQUIRED)
            if (WIN32)
                target_link_libraries(whisper-cpp::whisper-cpp INTERFACE CUDA::cudart_static CUDA::cublas CUDA::cublasLt CUDA::cuda_driver)
            else ()
                target_link_libraries(whisper-cpp::whisper-cpp INTERFACE CUDA::cudart_static CUDA::cublas_static CUDA::cublasLt_static CUDA::cuda_driver)
            endif()
        """)

    @property
    def _vulkan_build_module(self):
        return textwrap.dedent("""\
            find_dependency(Vulkan REQUIRED)
            target_link_libraries(whisper-cpp::whisper-cpp INTERFACE Vulkan::Vulkan)
        """)

    def config_options(self):
        if is_apple_os(self):
            del self.options.with_blas
            del self.options.with_cuda
            del self.options.with_vulkan  # Metal is preferred on Apple
        else:
            del self.options.metal
            del self.options.metal_ndebug
            del self.options.no_accelerate
            del self.options.with_coreml
            del self.options.coreml_allow_fallback

        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

        if is_apple_os(self):
            if not self.options.with_coreml:
                self.options.rm_safe("coreml_allow_fallback")

    def validate(self):
        if self.settings.compiler.cppstd:
            check_min_cppstd(self, self._min_cppstd)
        minimum_version = self._compilers_minimum_version.get(str(self.settings.compiler), False)
        if minimum_version and Version(self.settings.compiler.version) < minimum_version:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires C++{self._min_cppstd}, which your compiler does not support."
            )

    def requirements(self):
        if not is_apple_os(self):
            if self.options.get_safe("with_blas"):
                self.requires("openblas/0.3.24")
            if self.options.get_safe("with_vulkan"):
                # On Android, Vulkan library is provided by NDK/system but we need C++ headers
                if self.settings.os == "Android":
                    self.requires("vulkan-headers/1.3.290.0")
                else:
                    # On desktop, vulkan-loader brings in vulkan-headers transitively
                    self.requires("vulkan-loader/1.3.290.0")
        if self.options.get_safe("with_openvino"):
            self.requires("openvino/2023.2.0")

    def layout(self):
        cmake_layout(self, src_folder=".")

    def generate(self):
        deps = CMakeDeps(self)
        deps.set_property("openblas", "cmake_file_name", "BLAS")
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["WHISPER_BUILD_TESTS"] = False
        tc.variables["WHISPER_BUILD_EXAMPLES"] = False
        # Disable standalone mode to skip JavaScript bindings configuration
        tc.cache_variables["WHISPER_STANDALONE"] = "OFF"

        if self.options.shared:
            tc.variables["BUILD_SHARED_LIBS"] = True
        if self.options.sanitize_thread:
            tc.variables["WHISPER_SANITIZE_THREAD"] = True
        if self.options.sanitize_address:
            tc.variables["WHISPER_SANITIZE_ADDRESS"] = True
        if self.options.sanitize_undefined:
            tc.variables["WHISPER_SANITIZE_UNDEFINED"] = True
        if self.options.no_avx:
            tc.variables["WHISPER_NO_AVX"] = True
        if self.options.no_avx2:
            tc.variables["WHISPER_NO_AVX2"] = True
        if self.options.no_fma:
            tc.variables["WHISPER_NO_FMA"] = True
        if self.options.no_f16c:
            tc.variables["WHISPER_NO_F16C"] = True

        tc.variables["GGML_OPENMP"] = False
        tc.variables["GGML_CUDA"] = bool(self.options.get_safe("with_cuda", False))
        tc.variables["GGML_VULKAN"] = bool(self.options.get_safe("with_vulkan", False))

        # Android-specific fixes
        if self.settings.os == "Android":
            # pthread is built into libc, help CMake's FindThreads
            # Set ALL the variables FindThreads uses to skip its checks
            tc.cache_variables["CMAKE_THREAD_LIBS_INIT"] = ""
            tc.cache_variables["CMAKE_HAVE_THREADS_LIBRARY"] = "1"
            tc.cache_variables["CMAKE_USE_PTHREADS_INIT"] = "1"
            tc.cache_variables["THREADS_FOUND"] = "TRUE"
            tc.cache_variables["Threads_FOUND"] = "TRUE"
            tc.cache_variables["THREADS_PREFER_PTHREAD_FLAG"] = "ON"
            tc.cache_variables["CMAKE_HAVE_LIBC_PTHREAD"] = "1"

            # Help FindVulkan locate the library in NDK's API-level specific directory
            if self.options.get_safe("with_vulkan"):
                arch_map = {
                    "armv8": "aarch64-linux-android",
                    "armv7": "arm-linux-androideabi",
                    "x86": "i686-linux-android",
                    "x86_64": "x86_64-linux-android",
                }
                android_abi = arch_map.get(str(self.settings.arch), "aarch64-linux-android")
                api_level = int(str(self.settings.os.api_level))
                # Vulkan 1.1 requires minimum API level 29 (Android 10)
                # whisper.cpp uses Vulkan 1.1 features like vkGetPhysicalDeviceFeatures2
                vulkan_api_level = max(api_level, 29)
                # Get NDK path from Conan config and construct absolute sysroot path
                ndk_path = self.conf.get("tools.android:ndk_path")
                if ndk_path:
                    sysroot = os.path.join(ndk_path, "toolchains", "llvm", "prebuilt", "linux-x86_64", "sysroot")
                    tc.cache_variables["Vulkan_LIBRARY"] = os.path.join(sysroot, "usr", "lib", android_abi, str(vulkan_api_level), "libvulkan.so")
                # Use vulkan-headers from Conan (has C++ headers) instead of NDK (C only)
                vulkan_headers = self.dependencies["vulkan-headers"]
                vulkan_include = vulkan_headers.cpp_info.includedirs[0]
                tc.cache_variables["Vulkan_INCLUDE_DIR"] = vulkan_include

        # Desktop Linux Vulkan: Conan's FindVulkan.cmake doesn't provide glslc
        # We need glslc from system for shader compilation
        if self.settings.os == "Linux" and self.options.get_safe("with_vulkan"):
            import shutil
            glslc_path = shutil.which("glslc")
            if glslc_path:
                tc.cache_variables["Vulkan_GLSLC_EXECUTABLE"] = glslc_path
            else:
                self.output.warning("glslc not found in PATH. Vulkan shader compilation will fail. "
                                   "Install with: sudo apt install glslc")

        tc.variables["GGML_BLAS"] = bool(self.options.get_safe("with_blas", False))
        if self.options.get_safe("with_blas"):
            tc.variables["GGML_BLAS_VENDOR"] = "OpenBLAS"

        if self.options.get_safe("with_openvino"):
            tc.variables["WHISPER_OPENVINO"] = True
            tc.variables["CMAKE_CXX_STANDARD"] = str(self.settings.get_safe("compiler.cppstd", 11)).replace("gnu", "")

        if is_apple_os(self):
            if self.options.no_accelerate:
                tc.variables["WHISPER_NO_ACCELERATE"] = True
            if self.options.get_safe("metal_ndebug"):
                tc.variables["WHISPER_METAL_NDEBUG"] = True
            if self.options.with_coreml:
                tc.variables["WHISPER_COREML"] = True
                if self.options.coreml_allow_fallback:
                    tc.variables["WHISPER_COREML_ALLOW_FALLBACK"] = True
            tc.variables["GGML_METAL"] = bool(self.options.get_safe("metal", False))

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rm(self, "*.cmake", self.package_folder, recursive=True)
        rm(self, "*.pc", self.package_folder, recursive=True)

        # Save build modules for static linking with GPU backends
        if self.options.get_safe("with_cuda") and not self.options.shared:
            save(self, os.path.join(self.package_folder, "lib", "cmake", "whisper-cpp-cuda-static.cmake"), self._cuda_build_module)
        if self.options.get_safe("with_vulkan") and not self.options.shared:
            save(self, os.path.join(self.package_folder, "lib", "cmake", "whisper-cpp-vulkan-static.cmake"), self._vulkan_build_module)

    def package_info(self):
        self.cpp_info.libs = ["whisper"]
        self.cpp_info.libs.extend(["ggml", "ggml-base", "ggml-cpu"])

        if self.options.get_safe("with_cuda"):
            self.cpp_info.libs.append("ggml-cuda")
        if self.options.get_safe("with_vulkan"):
            self.cpp_info.libs.append("ggml-vulkan")
        self.cpp_info.resdirs = ["res"]

        if self.options.get_safe("with_blas"):
            self.cpp_info.libs.extend(["ggml-blas"])
        if self.options.get_safe("with_openvino"):
            self.cpp_info.requires.append("openvino::Runtime")
        if self.options.get_safe("with_vulkan") and self.settings.os != "Android":
            self.cpp_info.requires.append("vulkan-loader::vulkan-loader")

        if is_apple_os(self):
            if not self.options.no_accelerate:
                self.cpp_info.frameworks.append("Accelerate")
            if self.options.with_coreml:
                self.cpp_info.frameworks.append("CoreML")
            if self.options.get_safe("metal"):
                self.cpp_info.frameworks.extend(["CoreFoundation", "Foundation", "Metal", "MetalKit"])
                self.cpp_info.libs.extend(["ggml-metal"])
        elif self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.extend(["dl", "m", "pthread"])
        elif self.settings.os == "Android":
            self.cpp_info.system_libs.extend(["dl", "m", "log"])
            if self.options.get_safe("with_vulkan"):
                # Vulkan is a system library on Android
                self.cpp_info.system_libs.append("vulkan")
                # Add library search path via linker flags instead of libdirs
                # libdirs gets scanned by Qt's androiddeployqt and causes it to
                # bundle the NDK stub libvulkan.so, which crashes at runtime
                # Using linker flags avoids this while still allowing linking
                ndk_path = self.conf.get("tools.android:ndk_path")
                if ndk_path:
                    arch_map = {
                        "armv8": "aarch64-linux-android",
                        "armv7": "arm-linux-androideabi",
                        "x86": "i686-linux-android",
                        "x86_64": "x86_64-linux-android",
                    }
                    android_abi = arch_map.get(str(self.settings.arch), "aarch64-linux-android")
                    vulkan_lib_dir = os.path.join(ndk_path, "toolchains", "llvm", "prebuilt", "linux-x86_64", "sysroot", "usr", "lib", android_abi, "29")
                    self.cpp_info.sharedlinkflags.append(f"-L{vulkan_lib_dir}")
                    self.cpp_info.exelinkflags.append(f"-L{vulkan_lib_dir}")

        # Build modules for static linking
        build_modules = []
        if self.options.get_safe("with_cuda") and not self.options.shared:
            self.cpp_info.builddirs.append(os.path.join("lib", "cmake"))
            build_modules.append(os.path.join("lib", "cmake", "whisper-cpp-cuda-static.cmake"))
        if self.options.get_safe("with_vulkan") and not self.options.shared:
            self.cpp_info.builddirs.append(os.path.join("lib", "cmake"))
            build_modules.append(os.path.join("lib", "cmake", "whisper-cpp-vulkan-static.cmake"))
        if build_modules:
            self.cpp_info.set_property("cmake_build_modules", build_modules)

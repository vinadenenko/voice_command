# Vulkan Support Notes for whisper.cpp

This document summarizes the findings from debugging Vulkan GPU acceleration for whisper.cpp on desktop Linux and Android.

## Key Requirement

**whisper.cpp's GGML Vulkan backend requires Vulkan 1.2 or higher.**

The check is in `ggml-vulkan.cpp`:
```cpp
if (api_version < VK_API_VERSION_1_2) {
    std::cerr << "ggml_vulkan: Error: Vulkan 1.2 required." << std::endl;
    throw vk::SystemError(vk::Result::eErrorFeatureNotPresent, "Vulkan 1.2 required");
}
```

Vulkan 1.2 features used by the backend:
- `shaderFloat16` - Required for FP16 compute
- `bufferDeviceAddress` - Required for cooperative matrix operations
- `vulkanMemoryModel` - Memory model semantics

## Vulkan Version Encoding

Vulkan uses bit-shifting, not decimal encoding:

```c
#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
#define VK_API_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)
```

Reference values:
| Version | Encoded Value |
|---------|---------------|
| 1.1.0   | 4198400       |
| 1.2.0   | 4202496       |
| 1.3.0   | 4206592       |

Example: `apiVersion = 4198539` decodes to Vulkan 1.1.139 (< 1.2, not supported).

## Desktop Linux

### Build Requirements

1. **glslc shader compiler** - Conan's FindVulkan.cmake doesn't set `Vulkan_GLSLC_EXECUTABLE`
   ```bash
   sudo apt install glslc
   ```

2. **Conan recipe fix** - Must find system glslc and pass to CMake:
   ```python
   # In whisper-cpp conanfile.py generate()
   if self.settings.os == "Linux" and self.options.get_safe("with_vulkan"):
       import shutil
       glslc_path = shutil.which("glslc")
       if glslc_path:
           tc.cache_variables["Vulkan_GLSLC_EXECUTABLE"] = glslc_path
   ```

3. **vulkan-loader** from Conan provides both loader and headers for desktop.

## Android

### Build Requirements

1. **Vulkan 1.1 requires API level 29+** (Android 10)

2. **vulkan-headers from Conan** - NDK C headers don't include `vulkan.hpp` (C++ wrapper)
   ```python
   # In requirements()
   if self.settings.os == "Android":
       self.requires("vulkan-headers/1.3.290.0")
   else:
       self.requires("vulkan-loader/1.3.290.0")
   ```

3. **FindVulkan configuration** - Point CMake to NDK's Vulkan library:
   ```python
   # In generate()
   if self.settings.os == "Android" and self.options.get_safe("with_vulkan"):
       ndk_path = self.conf.get("tools.android:ndk_path")
       sysroot = os.path.join(ndk_path, "toolchains", "llvm", "prebuilt", "linux-x86_64", "sysroot")
       tc.cache_variables["Vulkan_LIBRARY"] = os.path.join(sysroot, "usr", "lib", android_abi, "29", "libvulkan.so")

       # Use Conan vulkan-headers instead of NDK headers
       vulkan_headers = self.dependencies["vulkan-headers"]
       tc.cache_variables["Vulkan_INCLUDE_DIR"] = vulkan_headers.cpp_info.includedirs[0]
   ```

### Critical: APK Bundling Issue

**Problem**: Qt's `androiddeployqt` scans `libdirs` from CMake targets and bundles any `.so` files it finds. If the NDK stub `libvulkan.so` (~25KB) gets bundled, the app crashes at startup with SIGSEGV during Qt JNI initialization.

**Root cause**: The NDK provides a stub `libvulkan.so` for linking. At runtime, Android apps must use the device's system Vulkan driver, not the stub.

**Solution**: Use linker flags instead of libdirs:
```python
# In package_info() - DO NOT use self.cpp_info.libdirs
if self.settings.os == "Android" and self.options.get_safe("with_vulkan"):
    self.cpp_info.system_libs.append("vulkan")
    ndk_path = self.conf.get("tools.android:ndk_path")
    if ndk_path:
        vulkan_lib_dir = os.path.join(ndk_path, "toolchains", "llvm", "prebuilt",
                                       "linux-x86_64", "sysroot", "usr", "lib",
                                       android_abi, "29")
        # Use linker flags - these are NOT scanned by androiddeployqt
        self.cpp_info.sharedlinkflags.append(f"-L{vulkan_lib_dir}")
        self.cpp_info.exelinkflags.append(f"-L{vulkan_lib_dir}")
```

### GGML Backend Loading on Android

Libraries are located in the app directory. Use `QCoreApplication::applicationDirPath()` to get the path.

```cpp
#include <ggml-backend.h>
#include <QCoreApplication>

void LoadGgmlBackends() {
    QString appDir = QCoreApplication::applicationDirPath();
    QByteArray pathBytes = appDir.toUtf8();

    // Load all backends from app directory
    ggml_backend_load_all_from_path(pathBytes.constData());

    // Or load specific backend
    QString vulkanPath = appDir + "/libggml-vulkan.so";
    ggml_backend_reg_t reg = ggml_backend_load(vulkanPath.toUtf8().constData());
}
```

### Debugging on Android

1. **GGML_LOG_DEBUG doesn't appear in logcat** - Use `qDebug()` instead
2. **std::cerr may not appear in logcat** - Vulkan 1.2 requirement error is silent
3. **Direct dlopen for error details**:
   ```cpp
   #include <dlfcn.h>

   dlerror(); // Clear existing error
   void* handle = dlopen(path, RTLD_NOW);
   if (!handle) {
       qDebug() << "dlopen failed:" << dlerror();
   }
   ```

4. **ADB commands for Vulkan diagnostics**:
   ```bash
   # Get full Vulkan capabilities JSON (features, extensions, limits)
   adb shell cmd gpu vkjson

   # Get GPU info summary
   adb shell dumpsys gpu

   # Filter Vulkan-specific info from dumpsys
   adb shell dumpsys gpu | grep -A 50 "Vulkan"

   # Check if Vulkan driver is present
   adb shell ls -la /vendor/lib64/hw/vulkan.*

   # List available Vulkan validation layers
   adb shell ls /data/local/debug/vulkan/
   ```

   The `vkjson` output includes:
   - `apiVersion` - Vulkan version (encoded integer, see decoding above)
   - `VkPhysicalDeviceFeatures` - core feature support
   - `VkPhysicalDevice16BitStorageFeatures` - 16-bit storage capabilities
   - `VkPhysicalDeviceShaderFloat16Int8Features` - FP16/INT8 shader support
   - Device extensions list

## GPU Compatibility

### Vulkan 1.2+ Support (Compatible)
- Mali-G76 and newer (2019+)
- Adreno 640 and newer
- Most Android devices from 2020+

### Vulkan 1.1 Only (NOT Compatible)
- Mali-G72 and older
- Adreno 630 and older
- Most Android devices before 2019

## Fallback: CPU-Only Inference

If Vulkan 1.2 is not available, whisper.cpp falls back to CPU inference. Performance tips:
- Use smaller models (tiny, base) instead of larger ones
- Use quantized models (Q4, Q5)
- Expect ~100 seconds for inference on mobile ARM processors with medium models

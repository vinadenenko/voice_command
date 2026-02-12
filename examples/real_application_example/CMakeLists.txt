cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(voice_command_test_app VERSION 0.1 LANGUAGES CXX)


find_package(Qt6 REQUIRED COMPONENTS Quick Multimedia)

# set(ENV{QT_ROOT} ${Qt6_DIR})
qt_standard_project_setup(REQUIRES 6.5)
# message("Set qt root: ${Qt6_DIR}")

find_package(voice_command REQUIRED)

qt_add_executable(appvoice_command_test_app
    main.cpp
)

qt_add_qml_module(appvoice_command_test_app
    URI Voice_command_test_app
    VERSION 1.0
    QML_FILES
        Main.qml
        RESOURCES conanfile.py
        SOURCES app.h app.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appvoice_command_test_app PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appvoice_command_test_app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appvoice_command_test_app
    PRIVATE
    voice_command::voice_command
    Qt6::Quick
    Qt6::Multimedia
)

# Fix: Conan creates INTERFACE IMPORTED targets, but Qt Android deployment only
# scans SHARED_LIBRARY targets. We manually add conan lib dirs so androiddeployqt finds them.
if(ANDROID)
    set_property(TARGET appvoice_command_test_app APPEND PROPERTY
        _qt_android_extra_library_dirs
        "${voice_command_LIB_DIRS_DEBUG}"
        "${whisper-cpp_LIB_DIRS_DEBUG}"
    )
endif()

include(GNUInstallDirs)
install(TARGETS appvoice_command_test_app
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

cmake_minimum_required(VERSION 3.21)
project(IBRA_PLAYER VERSION 1.0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network)
# libmpv: look up the header and import library directly. (mpv's pkg-config
# file on MSYS2 lists include folders that may not exist, which breaks
# IMPORTED_TARGET generation.)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MPV_PC QUIET mpv)
endif()
find_path(MPV_INCLUDE_DIR mpv/client.h HINTS ${MPV_PC_INCLUDE_DIRS} REQUIRED)
find_library(MPV_LIBRARY NAMES mpv libmpv mpv-2 HINTS ${MPV_PC_LIBRARY_DIRS} REQUIRED)
message(STATUS "libmpv: ${MPV_LIBRARY} (headers ${MPV_INCLUDE_DIR})")

set(SOURCES
    src/main.cpp
    src/engine/MpvWidget.cpp       src/engine/MpvWidget.h
    src/core/I18n.cpp              src/core/I18n.h
    src/core/AppSettings.cpp       src/core/AppSettings.h
    src/core/Library.cpp           src/core/Library.h
    src/core/Playlist.cpp          src/core/Playlist.h
    src/core/Log.cpp               src/core/Log.h
    src/core/Shortcuts.cpp         src/core/Shortcuts.h
    src/theme/Theme.cpp            src/theme/Theme.h
    src/ui/Icons.cpp               src/ui/Icons.h
    src/ui/GlassPanel.cpp          src/ui/GlassPanel.h
    src/ui/TitleBar.cpp            src/ui/TitleBar.h
    src/ui/ControlBar.cpp          src/ui/ControlBar.h
    src/ui/PlaylistPanel.cpp       src/ui/PlaylistPanel.h
    src/ui/MainWindow.cpp          src/ui/MainWindow.h
    src/ui/SettingsDialog.cpp      src/ui/SettingsDialog.h
    src/ui/ThemeStudio.cpp         src/ui/ThemeStudio.h
    src/ui/MediaInfoDialog.cpp     src/ui/MediaInfoDialog.h
    src/ui/AboutDialog.cpp         src/ui/AboutDialog.h
    src/platform/Platform.cpp      src/platform/Platform.h
    resources/ibra.qrc
)
if(WIN32)
    list(APPEND SOURCES resources/app.rc)
endif()

qt_add_executable(ibra-player WIN32 ${SOURCES})
target_include_directories(ibra-player PRIVATE src resources ${MPV_INCLUDE_DIR})
target_compile_definitions(ibra-player PRIVATE IBRA_VERSION="${PROJECT_VERSION}")
target_link_libraries(ibra-player PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Network
    ${MPV_LIBRARY})
if(WIN32)
    target_link_libraries(ibra-player PRIVATE dwmapi user32 shell32 advapi32)
endif()
set_target_properties(ibra-player PROPERTIES OUTPUT_NAME "iBRA PLAYER")

install(TARGETS ibra-player RUNTIME DESTINATION .)

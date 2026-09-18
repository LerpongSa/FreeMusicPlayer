cmake_minimum_required(VERSION 3.19)
project(FreeMusicPlayer VERSION 1.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Default to an optimized build. "MinGW Makefiles" (what build.bat configures
# below) is a single-config generator, where the actual optimization level
# comes from CMAKE_BUILD_TYPE at CONFIGURE time - unlike a multi-config
# generator (Visual Studio, Ninja Multi-Config), `cmake --build . --config
# Release` (what build.bat's build step passes) does NOT select this after
# the fact; for this generator, that flag is silently ignored. Since nothing
# ever set CMAKE_BUILD_TYPE at configure time, every build - the whole app,
# not just anything added since - was actually compiling with GCC's default
# -O0 (no optimization) despite build.bat's own "Release" wording implying
# otherwise. Confirmed via build/CMakeCache.txt: CMAKE_BUILD_TYPE was empty.
# This surfaced as "ISO-to-FLAC conversion is very slow" (2026-09-18) - that
# feature's per-sample Rice-coding search is exactly the kind of tight
# arithmetic loop -O0 vs -O3 differs most on - but it silently cost every
# other CPU-bound path in the app (FFT, biquad EQ, visualizer) the same way.
# `if(NOT ...)` still lets an explicit `-DCMAKE_BUILD_TYPE=Debug` (e.g. for
# gdb) override this default.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Release/Debug/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------
# IMPORTANT (see README.md "Build" section):
#   Qt's prebuilt mingw_64 libraries are bound to the exact MinGW compiler
#   shipped alongside them (C:\Qt\Tools\mingw1310_64). Configure with:
#
#   C:\Qt\6.11.1\mingw_64\bin\qt-cmake.bat -B build -G "Ninja" ^
#     -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"
#
#   Using qt-cmake alone is NOT enough - it does not force the compiler,
#   and picking up a different MinGW (WinLibs/MSYS2/TDM-GCC) produces
#   "undefined reference to `__imp___argc'" at link time.
# ---------------------------------------------------------------------------

if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.11.1/mingw_64" CACHE PATH "Qt6 install prefix")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_standard_project_setup()

set(APP_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/Settings.cpp
    src/Settings.h
    src/AudioEngine.cpp
    src/AudioEngine.h
    src/BiquadFilter.h
    src/Equalizer.cpp
    src/Equalizer.h
    src/FFT.h
    src/Visualizer.cpp
    src/Visualizer.h
    src/Playlist.cpp
    src/Playlist.h
    src/CoverArtExtractor.cpp
    src/CoverArtExtractor.h
    src/TagEditor.cpp
    src/TagEditor.h
    src/TagEditDialog.cpp
    src/TagEditDialog.h
    src/TextDecoder.cpp
    src/TextDecoder.h
    src/IconFactory.cpp
    src/IconFactory.h
    src/Theme.h
    src/FlacEncoder.cpp
    src/FlacEncoder.h
    src/DsfWriter.cpp
    src/DsfWriter.h
    src/IsoAudioExtractor.cpp
    src/IsoAudioExtractor.h
    src/IsoImportWorker.cpp
    src/IsoImportWorker.h
)

qt_add_executable(FreeMusicPlayer WIN32
    ${APP_SOURCES}
    resources/resources.qrc
)

if(WIN32)
    target_sources(FreeMusicPlayer PRIVATE resources/app.rc)
endif()

target_include_directories(FreeMusicPlayer PRIVATE src)

target_link_libraries(FreeMusicPlayer PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
)

if(WIN32)
    # Copy Qt DLLs + plugins next to the exe after each build (dev convenience).
    # windeployqt does NOT ship the MinGW runtime - see README "Portable packaging".
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "C:/Qt/6.11.1/mingw_64/bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET FreeMusicPlayer POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --dir "$<TARGET_FILE_DIR:FreeMusicPlayer>"
                    "$<TARGET_FILE:FreeMusicPlayer>"
            COMMENT "Running windeployqt"
        )
    endif()
endif()

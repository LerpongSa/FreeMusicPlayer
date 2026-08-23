@echo off
REM Deletes the build/ directory so the next build.bat run reconfigures
REM from scratch (needed after changing the compiler/generator, or if
REM CMake's cache gets into a bad state). Safe to run even if build/
REM doesn't exist yet.
if exist build (
    rmdir /s /q build
    echo Removed build\
) else (
    echo Nothing to clean - build\ does not exist.
)

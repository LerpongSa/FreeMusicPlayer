@echo off
REM ---------------------------------------------------------------------------
REM Always save a full copy of this run's output to build_log.txt AND keep the
REM console window open afterward (via `pause`), no matter how this script was
REM started. Without this, double-clicking build.bat in Explorer makes the
REM window close INSTANTLY the moment the script exits (success or failure) -
REM there is no time to read, let alone copy/paste, an error message. This
REM works by re-invoking this same script once with all of its output
REM redirected into build_log.txt, then printing that file back to the screen
REM and pausing before the window can close.
REM ---------------------------------------------------------------------------
if "%~1"=="__LOGGED__" goto :main

call "%~f0" __LOGGED__ > build_log.txt 2>&1
type build_log.txt
echo.
echo (Full output also saved to build_log.txt in this folder.)
pause
exit /b

:main
REM Builds FreeMusicPlayer with Qt 6.11.1's bundled MinGW toolchain.
REM
REM IMPORTANT: Qt's prebuilt mingw_64 libraries are bound to the exact
REM MinGW compiler shipped alongside them (C:\Qt\Tools\mingw1310_64, GCC
REM 13.1.0). A DIFFERENT MinGW - including MSYS2's C:\msys64\mingw64,
REM WinLibs, or TDM-GCC - will NOT work even if it happens to build:
REM linking against Qt's libs with the wrong compiler fails with
REM "undefined reference to `__imp___argc'" or similar ABI errors.
REM
REM This script uses the "MinGW Makefiles" generator (mingw32-make.exe,
REM which ships right next to g++.exe in the same Qt Tools folder) instead
REM of Ninja, because a default Qt installation does not include Ninja
REM unless that optional component was explicitly checked in the Qt
REM Maintenance Tool.

set QT_DIR=C:\Qt\6.11.1\mingw_64
set MINGW_DIR=C:\Qt\Tools\mingw1310_64
set BUILD_DIR=build

REM Forward-slash copy of MINGW_DIR, used only for the -D flags passed to
REM CMake below. CMake's resource-compiler detection (CMakeDetermineRC
REM Compiler / Windows-GNU.cmake) does NOT backslash-escape the path the way
REM the C/C++ compiler-ID step does when it writes it into
REM build\CMakeFiles\<ver>\CMakeRCCompiler.cmake - a path containing "\Q"
REM (as in C:\Qt\...) gets written out as a literal, unescaped "\Q", and
REM CMake's own string parser then reads that back as an invalid escape
REM sequence, failing configure with "Invalid character escape '\Q'". This
REM is a known CMake/MinGW-RC quirk (not specific to this project) - the
REM fix is to give CMake forward-slash paths instead, which it accepts
REM natively on Windows and writes out with no escaping ambiguity. Only
REM CMAKE_RC_COMPILER has actually hit this in practice, but all three -D
REM flags below use the forward-slash form for consistency.
set "MINGW_DIR_FWD=%MINGW_DIR:\=/%"

if not exist "%QT_DIR%\bin\qt-cmake.bat" (
    echo ERROR: Qt 6.11.1 mingw_64 kit not found at %QT_DIR%
    echo Open C:\Qt\MaintenanceTool.exe and check "Add or remove components"
    echo to make sure the MinGW 64-bit desktop kit AND the Qt Multimedia
    echo module are both installed.
    exit /b 1
)
if not exist "%MINGW_DIR%\bin\g++.exe" (
    echo ERROR: Qt's bundled MinGW compiler not found at %MINGW_DIR%
    echo.
    echo This is NOT the same as any other MinGW you might have installed
    echo separately ^(for example MSYS2 at C:\msys64\mingw64^) - Qt's
    echo prebuilt libraries only link correctly against the exact compiler
    echo Qt ships in its own Tools folder.
    echo.
    echo Fix: open C:\Qt\MaintenanceTool.exe -^> "Add or remove components"
    echo -^> under Qt 6.11.1, check "MinGW 13.1.0 64-bit" ^(or whichever
    echo MinGW version is listed there^) so it installs into
    echo C:\Qt\Tools\mingw1310_64, then run this script again.
    exit /b 1
)
if not exist "%MINGW_DIR%\bin\mingw32-make.exe" (
    echo ERROR: mingw32-make.exe not found at %MINGW_DIR%\bin
    echo Your Qt Tools MinGW install looks incomplete - reinstall the
    echo MinGW component via C:\Qt\MaintenanceTool.exe.
    exit /b 1
)
if not exist "%MINGW_DIR%\bin\windres.exe" (
    echo ERROR: windres.exe not found at %MINGW_DIR%\bin
    echo Your Qt Tools MinGW install looks incomplete - reinstall the
    echo MinGW component via C:\Qt\MaintenanceTool.exe.
    exit /b 1
)

REM Close a previously running build so the linker can overwrite the .exe.
tasklist /fi "imagename eq FreeMusicPlayer.exe" 2>nul | find /i "FreeMusicPlayer.exe" >nul
if not errorlevel 1 (
    echo Closing running FreeMusicPlayer.exe ...
    taskkill /f /im FreeMusicPlayer.exe >nul 2>&1
    ping -n 2 127.0.0.1 >nul
)

REM Self-healing: if build\ exists but was configured with a DIFFERENT
REM generator than the one this script uses (e.g. left over from an
REM earlier attempt that used Ninja), reusing it silently makes CMake
REM ignore the -G/-DCMAKE_MAKE_PROGRAM flags below and keep trying the old,
REM broken generator. Detect that and wipe it automatically instead of
REM requiring the user to remember to run clean.bat.
set NEED_CONFIGURE=0
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    set NEED_CONFIGURE=1
) else (
    findstr /C:"CMAKE_GENERATOR:INTERNAL=MinGW Makefiles" "%BUILD_DIR%\CMakeCache.txt" >nul
    if errorlevel 1 (
        echo Existing build\ was configured with a different generator ^(likely
        echo Ninja, from an earlier attempt^) - removing it and reconfiguring
        echo with MinGW Makefiles instead.
        rmdir /s /q "%BUILD_DIR%"
        set NEED_CONFIGURE=1
    )
)

REM Self-healing #2: CMake auto-detects the resource compiler (windres.exe)
REM the same way it would CMAKE_CXX_COMPILER if that weren't pinned above -
REM a plain PATH search - and this machine also has MSYS2 on PATH. An
REM earlier configure could easily have picked up MSYS2's windres.exe
REM instead of the one bundled with Qt's pinned MinGW. Mismatched
REM windres/gcc doesn't fail at configure time - it fails later, at BUILD
REM time, as "windres.exe: preprocessing failed" while compiling
REM resources/app.rc (windres shells out to a gcc.exe to preprocess the .rc
REM file, and pairing windres from one MinGW with gcc from another breaks
REM that step). Detect and wipe+reconfigure the same way as the generator
REM check above, this time also pinning -DCMAKE_RC_COMPILER explicitly so
REM it can't silently drift again on a later reconfigure.
if "%NEED_CONFIGURE%"=="0" (
    findstr /C:"CMAKE_RC_COMPILER:FILEPATH=" "%BUILD_DIR%\CMakeCache.txt" | findstr /I /C:"mingw1310_64" >nul
    if errorlevel 1 (
        echo Existing build\ has a resource compiler ^(windres.exe^) cached that
        echo is NOT Qt's bundled MinGW - likely auto-detected from MSYS2 or
        echo another MinGW on PATH. Removing build\ and reconfiguring with the
        echo correct windres.exe pinned, so the app icon compiles correctly.
        rmdir /s /q "%BUILD_DIR%"
        set NEED_CONFIGURE=1
    )
)

if "%NEED_CONFIGURE%"=="1" (
    echo Configuring with MinGW Makefiles ...
    REM MUST use `call` here: qt-cmake.bat is itself a .bat file, and invoking
    REM another .bat/.cmd from inside a batch script WITHOUT `call` hands
    REM control to it permanently instead of returning here when it finishes -
    REM doubly so from inside a parenthesized if-block like this one, and even
    REM more so now that this whole :main section is itself reached via a
    REM `call` from the self-relaunch wrapper above. Without `call` here, this
    REM entire block silently swallows everything after qt-cmake.bat finishes -
    REM including "echo Building ..." and the actual `cmake --build` step - and
    REM execution instead resumes back in the OUTER wrapper's type/pause lines,
    REM producing a build_log.txt that ends right after "Build files have been
    REM written to: ..." with the console still reporting a clean exit (no
    REM error, just an incomplete run) every single time.
    call "%QT_DIR%\bin\qt-cmake.bat" -B "%BUILD_DIR%" -G "MinGW Makefiles" ^
        -DCMAKE_CXX_COMPILER="%MINGW_DIR_FWD%/bin/g++.exe" ^
        -DCMAKE_RC_COMPILER="%MINGW_DIR_FWD%/bin/windres.exe" ^
        -DCMAKE_MAKE_PROGRAM="%MINGW_DIR_FWD%/bin/mingw32-make.exe"
    if errorlevel 1 (
        echo CMake configure failed - see the error above.
        exit /b 1
    )
)

echo Building ...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Build failed - see the error above.
    exit /b 1
)

echo.
echo Build succeeded: %BUILD_DIR%\FreeMusicPlayer.exe
echo (windeployqt runs automatically after each build to copy Qt DLLs next to it.)

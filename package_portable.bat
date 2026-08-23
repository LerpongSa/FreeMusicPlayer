@echo off
REM ---------------------------------------------------------------------------
REM Packages a portable, copy-anywhere-and-run FreeMusicPlayer folder out of
REM an existing build\ directory. Same log+pause wrapper as build.bat, for
REM the same reason: double-clicking a .bat in Explorer closes the console
REM window INSTANTLY on exit, success or failure, unless something pauses it.
REM ---------------------------------------------------------------------------
if "%~1"=="__LOGGED__" goto :main

call "%~f0" __LOGGED__ > package_log.txt 2>&1
type package_log.txt
echo.
echo (Full output also saved to package_log.txt in this folder.)
pause
exit /b

:main
REM Turns build\ (which already has FreeMusicPlayer.exe + Qt's DLLs/plugins,
REM copied there automatically by windeployqt as a CMake POST_BUILD step -
REM see CMakeLists.txt) into a clean, self-contained folder that can be
REM copied to a USB drive or another Windows PC and just double-clicked to
REM run - no Qt install, no installer, nothing else required on that PC.
REM
REM windeployqt copies Qt's own DLLs/plugins but deliberately does NOT copy
REM the MinGW C/C++ runtime the .exe itself was compiled against - without
REM those 3 DLLs the app fails to even start on a PC that doesn't happen to
REM have this exact MinGW installed. This script adds them explicitly.

set BUILD_DIR=build
set DIST_DIR=FreeMusicPlayer-Portable
set MINGW_DIR=C:\Qt\Tools\mingw1310_64

if not exist "%BUILD_DIR%\FreeMusicPlayer.exe" (
    echo ERROR: %BUILD_DIR%\FreeMusicPlayer.exe not found.
    echo Run build.bat first to build the app, then run this script again.
    exit /b 1
)

REM Close a running instance so nothing has the .exe/DLLs locked mid-copy.
tasklist /fi "imagename eq FreeMusicPlayer.exe" 2>nul | find /i "FreeMusicPlayer.exe" >nul
if not errorlevel 1 (
    echo Closing running FreeMusicPlayer.exe ...
    taskkill /f /im FreeMusicPlayer.exe >nul 2>&1
    ping -n 2 127.0.0.1 >nul
)

echo Packaging a portable copy into "%DIST_DIR%" ...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"

REM Copy everything windeployqt + the build produced (the .exe, Qt's DLLs,
REM and its plugin subfolders - platforms\, imageformats\, multimedia\,
REM styles\, etc.) EXCEPT CMake's own intermediate build files, which are
REM useless outside this machine and would only bloat the portable copy.
REM robocopy exit codes 0-7 mean success (some files copied / some skipped
REM because identical); 8+ means a real failure.
robocopy "%BUILD_DIR%" "%DIST_DIR%" /E /XD CMakeFiles /XF CMakeCache.txt cmake_install.cmake Makefile *.cmake *.o *.obj build.ninja .ninja_log .ninja_deps >nul
if errorlevel 8 (
    echo ERROR: robocopy failed while copying "%BUILD_DIR%" - see above.
    exit /b 1
)

REM The 3 MinGW runtime DLLs windeployqt does not ship - see comment above.
set MINGW_DLLS_OK=1
for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_DIR%\bin\%%D" (
        copy /y "%MINGW_DIR%\bin\%%D" "%DIST_DIR%\" >nul
    ) else (
        echo WARNING: %%D not found at %MINGW_DIR%\bin - the portable copy will not start on another PC without it.
        set MINGW_DLLS_OK=0
    )
)

REM Sanity-check the pieces a portable copy cannot run without, so a broken
REM bundle is caught here instead of on whatever other PC someone tries it
REM on next. Distinguishes "hard fail, will not even start" from "will
REM start but a feature is silently missing" per the checks below.
set MISSING=0
if not exist "%DIST_DIR%\FreeMusicPlayer.exe" ( echo MISSING: FreeMusicPlayer.exe & set MISSING=1 )
if not exist "%DIST_DIR%\platforms\qwindows.dll" ( echo MISSING: platforms\qwindows.dll - the app cannot start at all without this & set MISSING=1 )
if not exist "%DIST_DIR%\libgcc_s_seh-1.dll" ( echo MISSING: libgcc_s_seh-1.dll & set MISSING=1 )
if not exist "%DIST_DIR%\libstdc++-6.dll" ( echo MISSING: libstdc++-6.dll & set MISSING=1 )
if not exist "%DIST_DIR%\libwinpthread-1.dll" ( echo MISSING: libwinpthread-1.dll & set MISSING=1 )
if not exist "%DIST_DIR%\imageformats" ( echo WARNING: imageformats\ plugin folder missing - cover art / some icons may not load )
if not exist "%DIST_DIR%\multimedia" ( echo WARNING: multimedia\ plugin folder missing - audio playback will not work )

if "%MISSING%"=="1" (
    echo.
    echo Packaging finished but with MISSING required files - see above.
    echo Do NOT distribute "%DIST_DIR%" as-is; fix the issue and run this script again.
    exit /b 1
)

echo.
echo Portable copy ready: %CD%\%DIST_DIR%
echo.
echo Copy this whole folder anywhere - USB drive, another PC, zip it up and
echo send it - and double-click FreeMusicPlayer.exe inside it. No installer,
echo no Qt install needed on the other PC.
echo.
echo Settings/playlist/EQ/volume are saved as FreeMusicPlayer.ini right next
echo to the .exe (not the Windows registry), so the whole folder is fully
echo self-contained: moving or copying it elsewhere carries its settings
echo along, and never touches anything outside the folder itself.

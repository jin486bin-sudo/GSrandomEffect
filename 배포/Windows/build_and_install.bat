@echo off
setlocal

:: Auto-elevate to admin
>nul 2>&1 net session || (
    powershell -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
    exit /b
)

title GSrandom effect v2.0 Build and Install
cd /d "%~dp0"

echo.
echo ============================================
echo   GSrandom effect v2.0  Windows Installer
echo ============================================
echo.

:: Check CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found.
    echo   Install from: https://cmake.org/download/
    echo   Check "Add CMake to system PATH" during install.
    pause & exit /b 1
)

:: Check Git
where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git not found.
    echo   Install from: https://git-scm.com/
    pause & exit /b 1
)

:: Download JUCE if missing
if not exist "JUCE\CMakeLists.txt" (
    echo [1/4] Downloading JUCE 8.0.4...
    if exist "JUCE" rmdir /s /q "JUCE"
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
    if errorlevel 1 (
        echo [ERROR] JUCE download failed. Check internet connection.
        pause & exit /b 1
    )
) else (
    echo [1/4] JUCE already present, skipping.
)

:: Clean previous build
if exist "build_win" rmdir /s /q "build_win"

:: Configure - try VS 2022, 2019, 2017 in order
echo [2/4] Configuring...
cmake -B build_win -G "Visual Studio 17 2022" -A x64 >build_log.txt 2>&1
if errorlevel 1 (
    cmake -B build_win -G "Visual Studio 16 2019" -A x64 >build_log.txt 2>&1
)
if errorlevel 1 (
    cmake -B build_win -G "Visual Studio 15 2017 Win64" >build_log.txt 2>&1
)
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    echo   Install Visual Studio with "Desktop development with C++" workload.
    echo   Free: https://visualstudio.microsoft.com/downloads/
    echo.
    echo   Error log: %~dp0build_log.txt
    pause & exit /b 1
)

:: Build
echo [3/4] Building... (5-15 min, please wait)
cmake --build build_win --config Release >build_log.txt 2>&1
if errorlevel 1 (
    echo [ERROR] Build failed. See: %~dp0build_log.txt
    pause & exit /b 1
)

:: Install VST3
echo [4/4] Installing VST3...
set "SRC=%~dp0build_win\GSrandomEffect_artefacts\Release\VST3\GSrandom effect.vst3"
set "DST=%COMMONPROGRAMFILES%\VST3\GSrandom effect.vst3"

if not exist "%SRC%" (
    echo [ERROR] Build output not found: %SRC%
    pause & exit /b 1
)

if not exist "%COMMONPROGRAMFILES%\VST3" mkdir "%COMMONPROGRAMFILES%\VST3"
if exist "%DST%" rmdir /s /q "%DST%"
xcopy "%SRC%" "%DST%\" /e /i /y /q
if errorlevel 1 (
    echo [ERROR] Install failed.
    pause & exit /b 1
)

del /q build_log.txt >nul 2>&1

echo.
echo ============================================
echo   Done! Installed to:
echo   %DST%
echo   Restart your DAW to load the plugin.
echo ============================================
echo.
pause

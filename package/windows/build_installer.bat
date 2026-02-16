@echo off
setlocal enabledelayedexpansion

echo ======================================
echo OpenDAQ Qt GUI - Windows Installer Builder
echo ======================================
echo.

REM Get the script directory and navigate to project root
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%..\..\"
set "PROJECT_ROOT=%CD%"

REM Step 1: Build main application
echo Step 1: Building application...
cmake --preset opendaq-qt-gui -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build --preset opendaq-qt-gui --config Release

if errorlevel 1 (
    echo Failed to build application!
    exit /b 1
)

echo Application built successfully
echo.

REM Step 2: Configure package build
echo Step 2: Configuring installer package...
cd /d "%SCRIPT_DIR%"
cmake -B "%PROJECT_ROOT%\build\package" -S .

if errorlevel 1 (
    echo Failed to configure package!
    exit /b 1
)

echo Package configured
echo.

REM Step 3: Build package
echo Step 3: Preparing package...
cmake --build "%PROJECT_ROOT%\build\package" --config Release

if errorlevel 1 (
    echo Failed to prepare package!
    exit /b 1
)

echo Package prepared
echo.

REM Step 4: Create installer
echo Step 4: Creating installer...
cmake --build "%PROJECT_ROOT%\build\package" --config Release --target create_installer

if errorlevel 1 (
    echo Failed to create installer!
    exit /b 1
)

echo.
echo ======================================
echo Installer created successfully!
echo ======================================
echo.

REM Detect architecture
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCH=x64
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set ARCH=arm64
) else (
    set ARCH=x64
)

echo Installer location: build\package\OpenDAQ Qt GUI-1.0-Windows-%ARCH%.exe
echo.


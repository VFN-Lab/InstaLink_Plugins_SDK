@echo off
setlocal

echo ===================================================
echo   InstalinkSDK - Native Extension Builder (Windows)
echo ===================================================

if "%~1"=="" (
    echo Usage: build-native.bat ^<path_to_extension_folder^> [Debug^|Release]
    echo Example: build-native.bat ..\examples\native-echo
    exit /b 1
)

set "EXT_DIR=%~1"
set "BUILD_TYPE=%~2"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-NativeExtension.ps1" -ExtensionRoot "%EXT_DIR%" -BuildType "%BUILD_TYPE%"
if errorlevel 1 exit /b 1

echo.
echo SUCCESS! Native build completed. Outputs are in "%EXT_DIR%\runtime_bundle".
endlocal

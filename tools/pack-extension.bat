@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   InstalinkSDK - Extension Packager (Windows)
echo ===================================================

if "%~1"=="" (
    echo Usage: pack-extension.bat ^<path_to_extension_folder^>
    echo Example: pack-extension.bat ..\my-extension\
    exit /b 1
)

set EXT_DIR=%~1

if not exist "%EXT_DIR%\manifest.json" (
    echo Error: Could not find manifest.json in %EXT_DIR%
    exit /b 1
)

set DIST_DIR=%EXT_DIR%\dist
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

powershell -ExecutionPolicy Bypass -Command "& '%~dp0Pack-InstaLinkExtension.ps1' -ExtensionRoot '%EXT_DIR%'"

echo.
echo SUCCESS! Extension packaged successfully.
echo Look in the 'dist' folder inside your extension directory.
echo ===================================================
endlocal



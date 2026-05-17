@echo off
REM Dota 2 LAN Patch Verification Script
REM This script helps verify if the patch is correctly applied

echo ========================================
echo Dota 2 LAN Patch Verification
echo ========================================
echo.

REM Check if steamnetworkingsockets.dll exists
set "DOTA2_PATH=C:\Program Files (x86)\Steam\steamapps\common\dota 2 beta\game\bin\win64"
set "DLL_PATH=%DOTA2_PATH%\steamnetworkingsockets.dll"

if not exist "%DLL_PATH%" (
    echo [ERROR] steamnetworkingsockets.dll not found at:
    echo %DLL_PATH%
    echo.
    echo Please update DOTA2_PATH in this script to match your Dota 2 installation.
    pause
    exit /b 1
)

echo [OK] Found steamnetworkingsockets.dll
echo.

REM Check if GBE steam_api64.dll exists
set "GBE_DLL=%DOTA2_PATH%\steam_api64.dll"

if not exist "%GBE_DLL%" (
    echo [ERROR] steam_api64.dll not found at:
    echo %GBE_DLL%
    echo.
    echo Please copy the compiled GBE steam_api64.dll to the Dota 2 directory.
    pause
    exit /b 1
)

echo [OK] Found steam_api64.dll
echo.

REM Check for debug log
set "LOG_PATH=C:\Users\Public\gbe_gc_debug.log"

if exist "%LOG_PATH%" (
    echo [INFO] Found debug log at: %LOG_PATH%
    echo.
    echo Recent log entries:
    echo -------------------
    powershell -Command "Get-Content '%LOG_PATH%' -Tail 10"
    echo -------------------
    echo.
) else (
    echo [INFO] No debug log found yet. The log will be created when Dota 2 starts.
    echo.
)

echo ========================================
echo Verification Steps:
echo ========================================
echo.
echo 1. Start Dota 2 in offline mode
echo 2. Open console (press ~ or `)
echo 3. Type: net_option IP_AllowWithoutAuth
echo 4. Expected output: IP_AllowWithoutAuth = 1
echo.
echo If the value is 1, the patch is working correctly!
echo.
echo To create a LAN game:
echo   Server: map dota loopback=0
echo   Client: connect ^<server_ip^>
echo.
echo ========================================
echo.

pause

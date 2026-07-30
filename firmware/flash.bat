@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

set PYTHONIOENCODING=utf-8
set PIO=C:\Users\avivc\.platformio\penv\Scripts\pio.exe
if not exist "%PIO%" set PIO=pio

set PORT=%1
if "%PORT%"=="" set PORT=COM19

if "%PORT%"=="" (
    echo [ERROR] No COM port specified and no default configured for this project.
    echo Usage: flash.bat COMx
    echo.
    echo Available ports:
    "%PIO%" device list
    pause
    exit /b 1
)

REM This project also defines rgb_finder / diagtest / pinball / airhockey
REM environments -- this script only builds/flashes the main "esp32s3" firmware.
REM For the others: "%PIO%" run -e ^<name^> -t upload --upload-port %PORT%

echo ================================================
echo  Building Jukebox CODE  (esp32-s3-devkitc-1, env=esp32s3)
echo ================================================
"%PIO%" run -e esp32s3
if errorlevel 1 (
    echo.
    echo [FAILED] Build failed -- see errors above.
    pause
    exit /b 1
)

echo.
echo ================================================
echo  Flashing to %PORT%
echo ================================================
"%PIO%" run -e esp32s3 -t upload --upload-port %PORT%
if errorlevel 1 (
    echo.
    echo [FAILED] Upload failed -- see errors above.
    echo Check that the board is connected and %PORT% is correct.
    echo Usage: flash.bat [COM_PORT]
    pause
    exit /b 1
)

echo.
echo ================================================
echo  Done. Flashed successfully to %PORT%.
echo ================================================
pause

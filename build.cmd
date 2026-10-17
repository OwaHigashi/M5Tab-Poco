@echo off
REM Build wrapper for M5Tab-Poco.
REM Works around esp32:esp32 3.3.8 missing BOARD_SDMMC_POWER_CHANNEL in the
REM m5stack_tab5 variant header.
REM IMPORTANT: inject via compiler.*.extra_flags (appended) instead of
REM build.extra_flags (replaced) so board defaults like -DBOARD_HAS_PSRAM
REM and -DARDUINO_USB_MODE=1 are preserved.
REM   build.cmd                  -> compile, then list the COM ports (comports.ps1)
REM   build.cmd --upload -p COM4 -> compile + upload
setlocal
set "SKETCH_DIR=%~dp0"
if "%SKETCH_DIR:~-1%"=="\" set "SKETCH_DIR=%SKETCH_DIR:~0,-1%"
set "EXTRA=-DBOARD_SDMMC_POWER_CHANNEL=4"
arduino-cli compile ^
  --fqbn esp32:esp32:m5stack_tab5 ^
  --build-property "compiler.c.extra_flags=%EXTRA%" ^
  --build-property "compiler.cpp.extra_flags=%EXTRA%" ^
  --build-property "compiler.S.extra_flags=%EXTRA%" ^
  %* "%SKETCH_DIR%"
set "RC=%ERRORLEVEL%"
if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%SKETCH_DIR%\comports.ps1"
  echo   upload: build.cmd --upload -p COMx
)
endlocal & exit /b %RC%

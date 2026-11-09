@echo off
setlocal

REM ============================================================
REM  NgpCraft_base_template - Build script (system.lib path)
REM  MIT License
REM
REM  Uses the official Toshiba system.lib for flash save.
REM  (CLR_FLASH_RAM + WRITE_FLASH_RAM via bank-3 registers)
REM
REM  Use build.bat instead if you do NOT have system.lib --
REM  the standalone AMD stubs work without it.
REM
REM  CONFIGURATION: Edit the paths below to match your setup.
REM  "build_official_lib.bat nopause" never waits for a key.
REM
REM  Corrige campagne 2026-11-08 : same structure as build.bat (runs from its
REM  own folder, passes PYTHON to make, stops at the first error); the old
REM  ">/dev/null" redirections made cmd skip the MOVE commands of the resize.
REM ============================================================

REM --- Toolchain path (folder that contains BIN\cc900.exe) ---
SET compilerPath=C:\t900

REM --- Path to system.lib ---
REM     Option 1: place system.lib in the project root (auto-detected below).
REM     Option 2: set an explicit path here.
SET systemLibPath=
IF EXIST "%~dp0system.lib" SET systemLibPath=%~dp0system.lib

REM --- ROM name (also update CartTitle in src/core/carthdr.h) ---
SET romName=main

REM --- Pad ROM to 2MB for flash carts? (1=yes, 0=no) ---
SET ResizeRom=1

REM --- Launch emulator after build? (1=yes, 0=no) ---
SET Run=1

REM --- Emulator executable path (keep the quotes) ---
SET emuPath="C:\emu\NeoPop\NeoPop-Win32.exe"

REM ============================================================
REM  BUILD (do not edit below unless you know what you are doing)
REM ============================================================

cd /d "%~dp0"

SET THOME=%compilerPath%
SET BinPath=bin
SET romExt=ngc
SET ROM=%BinPath%\%romName%.%romExt%
path=%path%;%THOME%\bin
SET EXITCODE=0

echo [NgpCraft_base_template] Building %ROM% with system.lib ...

if "%systemLibPath%"=="" (
    echo.
    echo ERROR: system.lib not found.
    echo        Place system.lib in the project root or set systemLibPath above.
    echo        To build without system.lib, use build.bat instead.
    goto :fail
)
echo [NgpCraft_base_template] system.lib: %systemLibPath%

if not exist "%THOME%\BIN\cc900.exe" (
    echo.
    echo ERROR: cc900.exe not found in "%THOME%\BIN".
    echo        Set compilerPath at the top of this file.
    goto :fail
)
where make >nul 2>nul
if errorlevel 1 (
    echo.
    echo ERROR: "make" not found. It is not in "%THOME%\BIN" nor in PATH.
    goto :fail
)

SET PYTHON_CMD=
where py >nul 2>nul
if not errorlevel 1 SET PYTHON_CMD=py -3
if not defined PYTHON_CMD (
    where python >nul 2>nul
    if not errorlevel 1 SET PYTHON_CMD=python
)
if not defined PYTHON_CMD (
    echo.
    echo ERROR: Python 3 not found.
    goto :fail
)

SET MAKEARGS=NAME=%romName% "PYTHON=%PYTHON_CMD%" NGP_ENABLE_FLASH_SAVE=1 "SYSTEM_LIB=%systemLibPath%"

make -f makefile clean %MAKEARGS%
if errorlevel 1 goto :fail
make -f makefile %MAKEARGS%
if errorlevel 1 goto :fail
make -f makefile move_files %MAKEARGS%
if errorlevel 1 goto :fail
if not exist "%ROM%" goto :fail

if "%ResizeRom%"=="1" (
    if exist "utils\NGPRomResize.exe" (
        echo [NgpCraft_base_template] Resizing ROM to 2MB...
        MOVE "%BinPath%\%romName%.%romExt%" "%BinPath%\_%romName%.%romExt%" >nul 2>&1
        "utils\NGPRomResize.exe" "%BinPath%\_%romName%.%romExt%"
        MOVE "%BinPath%\_%romName%.%romExt%" "%BinPath%\%romName%.%romExt%" >nul 2>&1
        DEL "%BinPath%\MB__%romName%.%romExt%" >nul 2>&1
    ) else (
        echo [NgpCraft_base_template] Skip resize: utils\NGPRomResize.exe not found.
    )
)

echo.
echo ============================================================
echo  BUILD OK  -  %ROM%
echo ============================================================

if "%Run%"=="1" (
    if exist %emuPath% (
        echo [NgpCraft_base_template] Launching emulator...
        start "" %emuPath% "%~dp0%ROM%"
    ) else (
        echo [NgpCraft_base_template] Skip run: emulator not found at %emuPath%.
    )
)
goto :end

:fail
SET EXITCODE=1
echo.
echo ============================================================
echo  BUILD FAILED  -  no new ROM was made.
echo ============================================================

:end
if /i not "%~1"=="nopause" (
    echo %cmdcmdline% | find /i "%~nx0" >nul && pause
)
endlocal & exit /b %EXITCODE%

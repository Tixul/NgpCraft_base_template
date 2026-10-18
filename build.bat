@echo off
setlocal

REM ============================================================
REM  NgpCraft_base_template - Build script (Windows)
REM  MIT License
REM
REM  Flash save uses standalone AMD stubs (no system.lib needed).
REM  See build_official_lib.bat if you want the system.lib path.
REM
REM  CONFIGURATION: Edit the paths below to match your setup.
REM  Double-click this file, or run "build.bat" in a terminal.
REM  "build.bat nopause" never waits for a key (scripts, IDEs).
REM ============================================================

REM --- Toolchain path (folder that contains BIN\cc900.exe) ---
SET compilerPath=C:\t900

REM --- Flash save (standalone -- no system.lib required) ---
REM     0 = disabled (default)   1 = enabled
SET FlashSave=0

REM --- ROM name (also update CartTitle in src/core/carthdr.h) ---
SET romName=main

REM --- Pad ROM to 2MB for flash carts? (1=yes, 0=no) ---
SET ResizeRom=1

REM --- Launch emulator after a successful build? (1=yes, 0=no) ---
SET Run=1

REM --- Emulator executable path (keep the quotes) ---
SET emuPath="C:\emu\NeoPop\NeoPop-Win32.exe"

REM ============================================================
REM  BUILD (do not edit below unless you know what you are doing)
REM ============================================================

REM Always work from the folder of this file, wherever it was started from.
cd /d "%~dp0"

SET THOME=%compilerPath%
SET BinPath=bin
SET romExt=ngc
SET ROM=%BinPath%\%romName%.%romExt%
path=%path%;%THOME%\bin
SET EXITCODE=0

echo [NgpCraft_base_template] Building %ROM% ...

REM --- Check the tools before starting: a clear message beats a cryptic one ---
if not exist "%THOME%\BIN\cc900.exe" (
    echo.
    echo ERROR: cc900.exe not found in "%THOME%\BIN".
    echo        Set compilerPath at the top of build.bat to your Toshiba toolchain folder.
    goto :fail
)
where make >nul 2>nul
if errorlevel 1 (
    echo.
    echo ERROR: "make" not found. It is not in "%THOME%\BIN" nor in PATH.
    echo        Install GNU Make for Windows, or copy make.exe into "%THOME%\BIN".
    goto :fail
)

REM --- Find Python here and pass it to make (its own detection can fail under cmd) ---
SET PYTHON_CMD=
where py >nul 2>nul
if not errorlevel 1 SET PYTHON_CMD=py -3
if not defined PYTHON_CMD (
    where python >nul 2>nul
    if not errorlevel 1 SET PYTHON_CMD=python
)
if not defined PYTHON_CMD (
    echo.
    echo ERROR: Python 3 not found. Install it from python.org and tick
    echo        "Add python.exe to PATH" in the installer.
    goto :fail
)

SET MAKEARGS=NAME=%romName% "PYTHON=%PYTHON_CMD%"
if "%FlashSave%"=="1" SET MAKEARGS=%MAKEARGS% NGP_ENABLE_FLASH_SAVE=1

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
        echo                          Set emuPath at the top of build.bat, or Run=0.
    )
)
goto :end

:fail
SET EXITCODE=1
echo.
echo ============================================================
echo  BUILD FAILED  -  no new ROM was made.
echo  Read the FIRST error message above: the next ones follow from it.
echo ============================================================

:end
REM Keep the window open when the file was double-clicked.
if /i not "%~1"=="nopause" (
    echo %cmdcmdline% | find /i "%~nx0" >nul && pause
)
endlocal & exit /b %EXITCODE%

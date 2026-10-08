@echo off
setlocal
cd /d "%~dp0"
python build.py %*
exit /b %ERRORLEVEL%

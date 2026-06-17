@echo off
cd /d "%~dp0build"
set PATH=%~dp0build;%PATH%
MacDockShell.exe > "%~dp0run_path.log" 2>&1
echo EXITCODE=%ERRORLEVEL%>> "%~dp0run_path.log"

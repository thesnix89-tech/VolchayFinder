@echo off
cd /d "%~dp0build"
set QT_DEBUG_PLUGINS=1
MacDockShell.exe > "%~dp0run_debug.log" 2>&1
echo EXITCODE=%ERRORLEVEL%>> "%~dp0run_debug.log"

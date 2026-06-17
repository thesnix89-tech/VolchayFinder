@echo off
cd /d "%~dp0build"
set PATH=%~dp0build;%PATH%
set QT_DEBUG_PLUGINS=1
MacDockShell.exe > "%~dp0run_release.log" 2>&1
echo EXITCODE=%ERRORLEVEL%>> "%~dp0run_release.log"

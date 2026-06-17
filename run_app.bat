@echo off
cd /d "%~dp0build"
MacDockShell.exe > "%~dp0run.log" 2>&1
echo EXITCODE=%ERRORLEVEL%>> "%~dp0run.log"

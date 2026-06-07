@echo off
cd /d C:\Users\alexa\MacDockShell\build
set QT_DEBUG_PLUGINS=1
MacDockShell.exe > C:\Users\alexa\MacDockShell\run_debug.log 2>&1
echo EXITCODE=%ERRORLEVEL%>> C:\Users\alexa\MacDockShell\run_debug.log

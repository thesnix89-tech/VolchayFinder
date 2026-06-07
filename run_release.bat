@echo off
cd /d C:\Users\alexa\MacDockShell\build
set PATH=C:\Users\alexa\MacDockShell\build;%PATH%
set QT_DEBUG_PLUGINS=1
MacDockShell.exe > C:\Users\alexa\MacDockShell\run_release.log 2>&1
echo EXITCODE=%ERRORLEVEL%>> C:\Users\alexa\MacDockShell\run_release.log

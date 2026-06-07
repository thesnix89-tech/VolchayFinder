@echo off
cd /d C:\Users\alexa\MacDockShell\build
set PATH=C:\Users\alexa\MacDockShell\build;%PATH%
MacDockShell.exe > C:\Users\alexa\MacDockShell\run_path.log 2>&1
echo EXITCODE=%ERRORLEVEL%>> C:\Users\alexa\MacDockShell\run_path.log

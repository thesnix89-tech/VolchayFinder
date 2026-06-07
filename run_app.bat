@echo off
cd /d C:\Users\alexa\MacDockShell\build
MacDockShell.exe > C:\Users\alexa\MacDockShell\run.log 2>&1
echo EXITCODE=%ERRORLEVEL%>> C:\Users\alexa\MacDockShell\run.log

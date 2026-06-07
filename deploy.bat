@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
call "C:\Users\alexa\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" --qmldir C:\Users\alexa\MacDockShell\src\MacDockShell\qml C:\Users\alexa\MacDockShell\build\MacDockShell.exe

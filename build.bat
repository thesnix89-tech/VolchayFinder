@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
call "C:\Users\alexa\Qt\6.8.3\msvc2022_64\bin\qt-cmake.bat" -S "%~dp0." -B "%~dp0build" -G Ninja
if errorlevel 1 exit /b 1
cmake --build "%~dp0build" -j 4

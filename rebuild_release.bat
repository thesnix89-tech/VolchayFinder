@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
call "C:\Users\alexa\Qt\6.8.3\msvc2022_64\bin\qt-cmake.bat" -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build "%~dp0build" --clean-first -j 4
if errorlevel 1 exit /b 1
call "C:\Users\alexa\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" --release --qmldir "%~dp0src\MacDockShell\qml" "%~dp0build\MacDockShell.exe"

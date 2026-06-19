@echo off
setlocal

set "ROOT=%~dp0.."
set "EMOJI_SRC=%ROOT%\src\MacDockShell\fonts\emoji\AppleColorEmoji-Windows.ttf"
set "EMOJI_DST=%ROOT%\build\fonts\emoji\AppleColorEmoji-Windows.ttf"

if not exist "%EMOJI_SRC%" (
    echo Apple emoji font not found at %EMOJI_SRC%
    echo Run: powershell -ExecutionPolicy Bypass -File tools\setup_apple_emoji.ps1
    exit /b 0
)

if not exist "%ROOT%\build\fonts\emoji" mkdir "%ROOT%\build\fonts\emoji"
copy /Y "%EMOJI_SRC%" "%EMOJI_DST%" >nul
if errorlevel 1 (
    echo Failed to copy Apple emoji font to build output.
    exit /b 1
)
echo Copied Apple emoji font to %EMOJI_DST%

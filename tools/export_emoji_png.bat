@echo off
setlocal
python "%~dp0export_emoji_png.py"
exit /b %ERRORLEVEL%

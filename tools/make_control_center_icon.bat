@echo off
setlocal
cd /d "%~dp0.."
python "%~dp0make_control_center_icon.py"
if errorlevel 1 exit /b 1

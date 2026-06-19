@echo off
setlocal
set ROOT=%~dp0..
set QT_BIN=C:\Users\alexa\Qt\6.8.3\msvc2022_64\bin

if not exist "%QT_BIN%\lupdate.exe" (
    echo lupdate not found at %QT_BIN%
    exit /b 1
)

"%QT_BIN%\lupdate.exe" ^
    "%ROOT%\src\MacDockShell" ^
    "%ROOT%\src\MacDockShell\qml" ^
    "%ROOT%\src\MacDockShell\qml\components" ^
    -ts "%ROOT%\i18n\MacDockShell_en.ts" ^
       "%ROOT%\i18n\MacDockShell_uk.ts" ^
       "%ROOT%\i18n\MacDockShell_ru.ts"

if errorlevel 1 exit /b 1

"%QT_BIN%\lrelease.exe" "%ROOT%\i18n\MacDockShell_en.ts"
"%QT_BIN%\lrelease.exe" "%ROOT%\i18n\MacDockShell_uk.ts"
"%QT_BIN%\lrelease.exe" "%ROOT%\i18n\MacDockShell_ru.ts"

echo Translation files updated.

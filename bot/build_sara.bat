@echo off
echo Building SARA...
set PATH=C:\msys64\ucrt64\bin;%PATH%

cd /d "%~dp0..\build"
ninja

echo.
echo Build complete!
pause

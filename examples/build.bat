@echo off
setlocal

where gcc >nul 2>nul
if %errorlevel%==0 (
    gcc -std=c11 -Wall -Wextra -O2 -mwindows "%~dp0main.c" -lgdi32 -lwinmm -o "%~dp0example.exe"
    exit /b %errorlevel%
)

echo Could not find gcc in PATH.
exit /b 1

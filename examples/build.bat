@echo off
setlocal

where gcc >nul 2>nul
if %errorlevel%==0 (
    windres regions.rc -O coff -o regions.res
    gcc -std=c11 -Wall -Wextra -O2 -mwindows "%~dp0main.c" "%~dp0regions.res" -lgdi32 -lwinmm -o "%~dp0example.exe"
    exit /b %errorlevel%
)

echo Could not find cl.exe or gcc in PATH.
exit /b 1

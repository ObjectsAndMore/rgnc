@echo off
setlocal

where gcc >nul 2>nul
if %errorlevel%==0 (
    gcc -std=c11 -Wall -Wextra -O2 "%~dp0rgnc.c" -lpathcch -lgdi32 -o "%~dp0rgnc.exe"
    exit /b %errorlevel%
)

echo Could not find gcc in PATH.
exit /b 1

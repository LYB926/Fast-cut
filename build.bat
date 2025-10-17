@echo off
setlocal

if not exist build (
    mkdir build || exit /b 1
)

for /f "delims=" %%i in ('pkg-config --cflags gtk+-3.0 2^>nul') do set "GTK_CFLAGS=%%i"
if not defined GTK_CFLAGS (
    echo Failed to query GTK+ 3 build flags via pkg-config. >&2
    exit /b 1
)

for /f "delims=" %%i in ('pkg-config --libs gtk+-3.0 2^>nul') do set "GTK_LIBS=%%i"
if not defined GTK_LIBS (
    echo Failed to query GTK+ 3 linker flags via pkg-config. >&2
    exit /b 1
)

gcc -std=c11 -Wall -Wextra -O2 -mwindows %GTK_CFLAGS% src\fast_cut.c -o build\fast_cut.exe %GTK_LIBS%
if errorlevel 1 (
    echo Build failed. >&2
    exit /b 1
)

echo Created build\fast_cut.exe
exit /b 0

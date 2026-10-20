@echo off
:: ===========================================================================
::  build.bat — RDX Forensics & Cyber Investigation Language Build Script
::  Builds the 100% standalone executable (rdx.exe)
:: ===========================================================================
set "MINGW=C:\msys64\mingw64\bin"
set "PATH=%MINGW%;%PATH%"

set CFLAGS=-std=c11 -O2 -mconsole -static
set SRCS=src\main.c src\rdx_engine.c src\rdx_cyber.c src\rdx_diag.c

echo [rdx] Building Standalone RDX Forensics and Incident Response Language...
gcc %CFLAGS% -o rdx.exe %SRCS%
if errorlevel 1 (
    echo [rdx] BUILD FAILED
    exit /b 1
)
copy /y rdx.exe rdx_bin.exe >nul
echo [rdx] Build successful: rdx.exe and rdx_bin.exe

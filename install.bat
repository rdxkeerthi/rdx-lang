@echo off
REM ==============================================================================
REM  RDX Language Windows Installer Launcher
REM ==============================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
if errorlevel 1 (
    echo.
    echo [ERROR] RDX Installation encountered an issue.
    pause
    exit /b 1
)

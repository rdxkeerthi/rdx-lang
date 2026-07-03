@echo off
REM ════════════════════════════════════════════════════════════════
REM  install.bat — RDX Language Installer for Windows  v2.0
REM  Registers .rdx file extension, icon, and PATH entry
REM  Run as Administrator for HKLM-wide registration
REM ════════════════════════════════════════════════════════════════
setlocal

set "RDX_DIR=%~dp0"
REM Remove trailing backslash
if "%RDX_DIR:~-1%"=="\" set "RDX_DIR=%RDX_DIR:~0,-1%"

set "RDX_EXE=%RDX_DIR%\rdx_bin.exe"
set "RDX_ICON=%RDX_DIR%\rdx-icon.svg"
set "RDX_BAT=%RDX_DIR%\rdx.bat"

echo.
echo ╔═══════════════════════════════════════════╗
echo ║   RDX Language Installer  v2.0            ║
echo ║   The Honeybee Systems Language  🐝        ║
echo ╚═══════════════════════════════════════════╝
echo.

REM ── Step 0: Build if not present ─────────────────────────────
if not exist "%RDX_EXE%" (
    echo [0/5] rdx_bin.exe not found. Building now...
    call "%RDX_DIR%\build.bat"
    if errorlevel 1 (
        echo [ERROR] Build failed. Cannot install.
        exit /b 1
    )
    echo.
) else (
    echo [0/5] rdx_bin.exe found: %RDX_EXE%
)

REM ── Step 1: Register .rdx file extension ─────────────────────
echo [1/5] Registering .rdx file extension...
reg add "HKCU\Software\Classes\.rdx"                          /ve /d "rdxfile"        /f >nul 2>&1
reg add "HKCU\Software\Classes\rdxfile"                       /ve /d "RDX Source File" /f >nul 2>&1
reg add "HKCU\Software\Classes\rdxfile\DefaultIcon"           /ve /d "%RDX_ICON%,0"   /f >nul 2>&1
echo     Done.

REM ── Step 2: Register Open With rdx_bin.exe ───────────────────
echo [2/5] Registering Open action...
reg add "HKCU\Software\Classes\rdxfile\shell\open"            /ve /d "Run with RDX"   /f >nul 2>&1
reg add "HKCU\Software\Classes\rdxfile\shell\open\command"    /ve /d "\"%RDX_EXE%\" \"%%1\"" /f >nul 2>&1
echo     Done.

REM ── Step 3: Create rdx.bat launcher ──────────────────────────
echo [3/5] Creating rdx.bat launcher...
(
    echo @echo off
    echo "%RDX_EXE%" %%*
) > "%RDX_BAT%"
echo     Created: %RDX_BAT%

REM ── Step 4: Add to user PATH ──────────────────────────────────
echo [4/5] Updating user PATH...
for /f "tokens=2*" %%a in ('reg query "HKCU\Environment" /v PATH 2^>nul') do set "USER_PATH=%%b"
echo %USER_PATH% | findstr /i "%RDX_DIR%" >nul 2>&1
if errorlevel 1 (
    reg add "HKCU\Environment" /v PATH /d "%USER_PATH%;%RDX_DIR%" /f >nul 2>&1
    echo     Added %RDX_DIR% to user PATH.
    echo     NOTE: Restart your terminal for PATH to take effect.
) else (
    echo     RDX already in PATH.
)

REM ── Step 5: Smoke test ───────────────────────────────────────
echo [5/5] Running smoke test...
"%RDX_EXE%" "%RDX_DIR%\examples\fibonacci.rdx" >nul 2>&1
if errorlevel 1 (
    "%RDX_EXE%" "%RDX_DIR%\examples\greet.rdx" >nul 2>&1
    if errorlevel 1 (
        echo [WARN] Smoke test returned errors. Check rdx_bin.exe manually.
    ) else (
        echo     Smoke test passed: greet.rdx
    )
) else (
    echo     Smoke test passed: fibonacci.rdx
)

echo.
echo ╔═══════════════════════════════════════════╗
echo ║   Installation Complete!                  ║
echo ║                                           ║
echo ║   Quick start:                            ║
echo ║     rdx examples\greet.rdx               ║
echo ║     rdx examples\fibonacci.rdx           ║
echo ║     rdx examples\prime.rdx               ║
echo ║     rdx examples\todo_app.rdx            ║
echo ║     rdx rdxc.rdx                         ║
echo ║                                           ║
echo ║   Standard library:                       ║
echo ║     rdx lib\math.rdx                     ║
echo ║     rdx lib\str.rdx                      ║
echo ║     rdx lib\io.rdx                       ║
echo ║     rdx lib\sys.rdx                      ║
echo ║                                           ║
echo ║   Restart terminal for PATH changes.      ║
echo ╚═══════════════════════════════════════════╝
echo.

pause

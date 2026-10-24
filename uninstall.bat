@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo                      RDX Language Uninstaller
echo ==============================================================================

set "INSTALL_DIR=%LOCALAPPDATA%\rdx\bin"

echo [1/3] Removing from User PATH...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$target = [System.IO.Path]::GetFullPath('%INSTALL_DIR%'); " ^
    "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User'); " ^
    "if ($userPath) { " ^
    "    $paths = $userPath -split ';' | Where-Object { $_ -ne '' -and $_ -ne $target -and $_ -ne '%INSTALL_DIR%' }; " ^
    "    $newPath = $paths -join ';'; " ^
    "    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User'); " ^
    "    Write-Host '      Removed from User PATH.'; " ^
    "}"

echo [2/3] Removing .rdx file registry associations...
reg delete "HKCU\Software\Classes\.rdx" /f >nul 2>&1
reg delete "HKCU\Software\Classes\rdxfile" /f >nul 2>&1

echo [3/3] Removing installed files...
if exist "%INSTALL_DIR%" (
    rmdir /s /q "%LOCALAPPDATA%\rdx" >nul 2>&1
    echo       Removed %LOCALAPPDATA%\rdx
)

echo.
echo RDX has been successfully uninstalled.
echo.

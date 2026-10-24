# ==============================================================================
#  install.ps1 — RDX Language PowerShell Installer (Windows)
# ==============================================================================
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host "                RDX Language Standalone Installer (v3.0.0)" -ForegroundColor Cyan
Write-Host "        Syntax-Free * Zero-Dependency * Cyber Forensics Runtime" -ForegroundColor Cyan
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host ""

$srcDir = $PSScriptRoot
if (-not $srcDir) { $srcDir = Get-Location }

$installDir = Join-Path $env:LOCALAPPDATA "rdx\bin"
$targetExe = Join-Path $installDir "rdx.exe"

# 1. Compile if needed
Write-Host "[1/5] Checking executable..." -ForegroundColor Yellow
$srcExe = Join-Path $srcDir "rdx.exe"
if (-not (Test-Path $srcExe)) {
    Write-Host "      Compiling rdx.exe..." -ForegroundColor Gray
    Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$srcDir\build.bat`"" -NoNewWindow -Wait
}

if (-not (Test-Path $srcExe)) {
    Write-Error "Compilation failed. Could not find $srcExe"
}
Write-Host "      Binary verified: $srcExe" -ForegroundColor Green

# 2. Copy to install directory
Write-Host "[2/5] Installing to $installDir..." -ForegroundColor Yellow
if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}
Copy-Item -Path $srcExe -Destination $targetExe -Force
Copy-Item -Path $srcExe -Destination (Join-Path $installDir "rdx_bin.exe") -Force

# Create wrapper batch
$wrapperContent = "@echo off`r`n`"$targetExe`" %*`r`n"
Set-Content -Path (Join-Path $installDir "rdx.cmd") -Value $wrapperContent -Encoding Ascii

Write-Host "      Copied rdx.exe, rdx_bin.exe, and rdx.cmd to $installDir" -ForegroundColor Green

# 3. Add to User PATH
Write-Host "[3/5] Updating User PATH..." -ForegroundColor Yellow
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if (-not $userPath) { $userPath = "" }
$paths = $userPath -split ';' | Where-Object { $_ -ne "" }

if ($paths -notcontains $installDir) {
    $newPath = ($paths + $installDir) -join ';'
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "      Added $installDir to User PATH." -ForegroundColor Green
} else {
    Write-Host "      $installDir is already in User PATH." -ForegroundColor Gray
}

# Update current session PATH
$env:Path = "$installDir;$env:Path"

# 4. Register file association
Write-Host "[4/5] Registering .rdx file extension..." -ForegroundColor Yellow
try {
    New-Item -Path "HKCU:\Software\Classes\.rdx" -Value "rdxfile" -Force | Out-Null
    New-Item -Path "HKCU:\Software\Classes\rdxfile" -Value "RDX Language Source File" -Force | Out-Null
    New-Item -Path "HKCU:\Software\Classes\rdxfile\shell\open\command" -Value "`"$targetExe`" `"%1`"" -Force | Out-Null
    Write-Host "      Registered .rdx file extension association." -ForegroundColor Green
} catch {
    Write-Host "      Note: Could not register registry extension (non-fatal)." -ForegroundColor Gray
}

# 5. Smoke test
Write-Host "[5/5] Running verification smoke test..." -ForegroundColor Yellow
$helloFile = Join-Path $srcDir "examples\hello_world.rdx"
if (Test-Path $helloFile) {
    & $targetExe $helloFile | Out-Null
    Write-Host "      Smoke test passed successfully!" -ForegroundColor Green
}

Write-Host ""
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host "                      RDX INSTALLATION SUCCESSFUL!                            " -ForegroundColor Cyan
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  You can now open ANY terminal and run:" -ForegroundColor White
Write-Host "    rdx test.rdx         (Run any script from any directory)" -ForegroundColor White
Write-Host "    rdx                  (Launch the interactive REPL)" -ForegroundColor White
Write-Host ""

# run_zeebx.ps1 - Launch unpacked Resident Evil 1 Zeebo build in zeebx-headless.exe
param(
    [string]$GamePath = "$PSScriptRoot\..\..\dist\zeebo\re1zeebo\re1zeebo.mod"
)

$ZeebxExe = Join-Path $PSScriptRoot "..\zeebx-emu\zeebx-headless.exe"
if (-not (Test-Path $ZeebxExe)) {
    Write-Error "zeebx-headless.exe not found at $ZeebxExe"
    exit 1
}

if (-not (Test-Path $GamePath)) {
    Write-Warning "Module not found at $GamePath."
    Write-Host "Build re1zeebo.mod and run tools/zeebo/package_mod.py first."
    exit 1
}

& $ZeebxExe $GamePath

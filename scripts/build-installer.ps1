<#
 Rebuilds the Windows installer (EXE) from an already built PRYSM.vst3 bundle.
 Usage:  .\scripts\build-installer.ps1 [-Vst3Dir build\PRYSM_artefacts\Release\VST3\PRYSM.vst3] [-OutDir dist] [-Version 1.0.0]
 Requires NSIS 3 (makensis.exe in PATH or in "C:\Program Files (x86)\NSIS").
#>
param(
    [string]$Vst3Dir = "build\PRYSM_artefacts\Release\VST3\PRYSM.vst3",
    [string]$OutDir  = "dist",
    [string]$Version = "1.0.0"
)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
$makensis = (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
if (-not $makensis -and (Test-Path "${env:ProgramFiles(x86)}\NSIS\makensis.exe")) { $makensis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe" }
if (-not $makensis) { throw "makensis.exe not found. Install NSIS 3 (https://nsis.sourceforge.io)." }
$vst3 = (Resolve-Path $Vst3Dir).Path
if (-not (Test-Path "$vst3\Contents\x86_64-win\PRYSM.vst3")) { throw "Missing $vst3\Contents\x86_64-win\PRYSM.vst3 - build the plug-in first." }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$out = (Resolve-Path $OutDir).Path
$log = & $makensis /V2 "/DVERSION=$Version" "/DVST3_DIR=$vst3" "/DOUT_DIR=$out" installer\prysm.nsi 2>&1 | Out-String
Write-Host $log
if ($LASTEXITCODE -ne 0) {
    if ($env:GITHUB_ACTIONS) { $one = ($log -replace "`r?`n", " | "); Write-Host "::error title=makensis failed::$one" }
    throw "makensis failed ($LASTEXITCODE)"
}
$exe = Join-Path $out "PRYSM-$Version-Windows-x64-Setup.exe"
if (-not (Test-Path $exe)) { throw "Installer was not produced: $exe" }
Get-Item $exe | Format-List Name, Length

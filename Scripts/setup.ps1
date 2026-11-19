# One-time setup after cloning: build the editor, import the Fab toy car, create the car presets.
#   Scripts\setup.ps1 [-Engine <UE 5.8 root>]
# Needs: UE 5.8, Visual Studio 2022 Build Tools, the Fab toy car downloaded, Plugins\MetaXR copied in (see README).
param([string]$Engine = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'D:\Epic Games\UE_5.8' }))
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path (Join-Path $root 'Plugins\MetaXR\OculusXR.uplugin'))) {
    Write-Warning 'Plugins\MetaXR is missing: the game will build, but mixed reality runs as full VR without passthrough. See README.'
}

$logs = Join-Path $root 'Build\logs'
New-Item -ItemType Directory -Force $logs | Out-Null
Write-Output 'Building the editor...'
& (Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat') FormulaXREditor Win64 Development "-Project=$root\FormulaXR.uproject" -WaitMutex *> (Join-Path $logs 'build_editor.log')
if ($LASTEXITCODE -ne 0) { throw "Editor build failed; see $logs\build_editor.log" }

& (Join-Path $PSScriptRoot 'import_toy_car.ps1') -Engine $Engine
& (Join-Path $PSScriptRoot 'run_editor_script.ps1') create_car_presets CARPRESETS_OK -Engine $Engine
Write-Output 'Setup done. Run launch.bat (PC) or launch-vr.bat (Quest 3), or build.bat to package.'

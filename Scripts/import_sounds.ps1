# Generate the sound effects (Scripts/generate_sounds.py) and import them to /Game/FormulaXR/Audio.
#   Scripts\import_sounds.ps1 [-Engine <UE root>]
param([string]$Engine = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'D:\Epic Games\UE_5.8' }))
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& python -I (Join-Path $PSScriptRoot 'generate_sounds.py') (Join-Path $root 'Build\audio')
if ($LASTEXITCODE -ne 0) { throw 'Sound generation failed.' }
& (Join-Path $PSScriptRoot 'run_editor_script.ps1') import_sounds SOUNDS_IMPORT_OK -Engine $Engine

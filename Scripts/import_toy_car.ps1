# Copy the Fab toy car into SourceArt/ and import it (meshes, textures, materials) plus the empty map.
#   Scripts\import_toy_car.ps1 [-Engine <UE root>]
# The editor binaries must be built first (Build.bat FormulaXREditor Win64 Development).
param([string]$Engine = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'D:\Epic Games\UE_5.8' }))
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$fab = 'C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary\Toy_Racing_Car_Low-poly_PBR-e29223c8\fbx\car_extracted\car'
$source = Join-Path $root 'SourceArt\ToyRacingCar'

if (-not (Test-Path (Join-Path $source 'car.fbx'))) {
    if (-not (Test-Path $fab)) { throw "Toy car not found in the Fab cache: $fab. Download it from Fab > My Library." }
    New-Item -ItemType Directory -Force (Join-Path $source 'textures') | Out-Null
    Copy-Item (Join-Path $fab 'car.fbx') $source
    Copy-Item (Join-Path $fab 'textures\*.png') (Join-Path $source 'textures')
}

$logs = Join-Path $root 'Build\logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$log = Join-Path $logs 'import_toy_car.log'
$p = Start-Process -FilePath (Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList @(
    "`"$root\FormulaXR.uproject`"", "-ExecutePythonScript=`"$root\Scripts\import_toy_car.py`"",
    '-unattended', '-nosplash', '-nopause', '-nohmd', "-abslog=`"$log`"") -Wait -PassThru -WindowStyle Hidden
Select-String -LiteralPath $log -Pattern 'TOYCAR' | ForEach-Object { $_.Line }
if ($p.ExitCode -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'TOYCAR_IMPORT_OK' -Quiet) -or
    (Select-String -LiteralPath $log -Pattern 'TOYCAR_IMPORT_FAILED' -Quiet)) {
    Select-String -LiteralPath $log -Pattern 'Error|Traceback' | Select-Object -First 15 | ForEach-Object { $_.Line }
    throw "Toy car import failed; see $log"
}
Write-Output 'Toy car imported.'

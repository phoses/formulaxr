# Run one editor Python script headless and check it printed its OK marker.
#   Scripts\run_editor_script.ps1 <script name without .py> <OK marker>
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Ok,
    [string]$Engine = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'D:\Epic Games\UE_5.8' })
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$logs = Join-Path $root 'Build\logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$log = Join-Path $logs "$Name.log"
$p = Start-Process -FilePath (Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList @(
    "`"$root\FormulaXR.uproject`"", "-ExecutePythonScript=`"$root\Scripts\$Name.py`"",
    '-unattended', '-nosplash', '-nopause', '-nohmd', "-abslog=`"$log`"") -Wait -PassThru -WindowStyle Hidden
if ($p.ExitCode -ne 0 -or -not (Select-String -LiteralPath $log -Pattern $Ok -Quiet)) {
    Select-String -LiteralPath $log -Pattern 'Error|Traceback' | Select-Object -First 15 | ForEach-Object { $_.Line }
    throw "$Name failed; see $log"
}
Write-Output "$Name OK"

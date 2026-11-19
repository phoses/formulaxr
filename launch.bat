@echo off
rem FormulaXR in normal PC mode. Double-click to play. Needs an Xbox controller.
rem Runs the packaged game (Build\Windows) if it exists, otherwise the project through the editor.
setlocal
set "ROOT=%~dp0"
if not defined UE_ROOT set "UE_ROOT=D:\Epic Games\UE_5.8"
rem -nohmd: never start the headset in PC mode, even if a Quest is connected.
set "ARGS=-nohmd -fullscreen -nosplash"
if exist "%ROOT%Build\Windows\FormulaXR.exe" (
  start "" "%ROOT%Build\Windows\FormulaXR.exe" %ARGS%
) else if exist "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" (
  start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%ROOT%FormulaXR.uproject" -game %ARGS%
) else (
  echo Unreal Engine not found at "%UE_ROOT%". Set UE_ROOT to your UE 5.8 folder.
  pause
)

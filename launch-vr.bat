@echo off
rem FormulaXR in mixed reality on Quest 3 over Meta Horizon Link (cable or Air Link).
rem Runs the packaged game (Build\Windows) if it exists, otherwise the project through the editor.
setlocal
set "ROOT=%~dp0"
if not defined UE_ROOT set "UE_ROOT=D:\Epic Games\UE_5.8"

rem Preflight: Meta Horizon Link must be the active OpenXR runtime.
reg query "HKLM\SOFTWARE\Khronos\OpenXR\1" /v ActiveRuntime 2>nul | findstr /i "oculus" >nul
if errorlevel 1 (
  echo WARNING: Meta Horizon Link is not the active OpenXR runtime.
  echo Open the Meta Horizon Link app and set it as the OpenXR runtime, then try again.
  pause
)
rem Preflight: the Link service should be running.
tasklist /fi "imagename eq OVRServer_x64.exe" 2>nul | findstr /i "OVRServer" >nul
if errorlevel 1 (
  echo WARNING: Meta Horizon Link does not seem to be running. Start it and connect the Quest 3.
  pause
)

echo Put on your Quest 3 with Link active. Right stick: track height, right stick press: re-center.
set "ARGS=-vr -windowed -ResX=1280 -ResY=720 -nosplash"
if exist "%ROOT%Build\Windows\FormulaXR.exe" (
  start "" "%ROOT%Build\Windows\FormulaXR.exe" %ARGS%
) else if exist "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" (
  start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%ROOT%FormulaXR.uproject" -game %ARGS%
) else (
  echo Unreal Engine not found at "%UE_ROOT%". Set UE_ROOT to your UE 5.8 folder.
  pause
)

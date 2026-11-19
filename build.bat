@echo off
rem Packages a Windows Shipping build into Build\Windows (used by launch.bat and launch-vr.bat).
setlocal
set "ROOT=%~dp0"
if not defined UE_ROOT set "UE_ROOT=D:\Epic Games\UE_5.8"
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%ROOT%FormulaXR.uproject" ^
  -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive ^
  -archivedirectory="%ROOT%Build" -unattended -utf8output
if errorlevel 1 (
  echo Packaging failed.
  pause
  exit /b 1
)
echo Packaged to %ROOT%Build\Windows

@echo off
title Avatar Studio - Interactive Model Lab
cd /d "%~dp0"
echo Avatar Studio: native MOC3 renderer, interactive inspector and Physics3.
echo Model files, presets and captures remain local. Use --legacy for original Viewer.
python "%~dp0scripts\layer_physics_studio.py" %*
if errorlevel 1 (
  echo Avatar Studio could not finish. Review the local error above.
  pause
  exit /b 1
)

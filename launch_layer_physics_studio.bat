@echo off
title Layer Physics Studio - Native MOC3 Viewer
cd /d "%~dp0"
echo Layer Physics Studio: native raylib interface, local model assets only.
echo Presets and screenshots remain in build/studio-user.
python "%~dp0scripts\layer_physics_studio.py" %*
if errorlevel 1 (
  echo Layer Physics Studio could not finish. Review the local error above.
  pause
  exit /b 1
)

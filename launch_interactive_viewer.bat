@echo off
title Avatar Studio - Interactive Model Lab
cd /d "%~dp0"
echo Avatar Studio: expressions, linked Physics3, motion and model inspector
echo Mouse: left pull / right pan / wheel zoom. Space: pause. 1-9: expression.
echo P: physics. R: fit. F12: capture. Presets and PNGs stay in build/studio-user.
echo Use --legacy to open the original viewer, or --model "path.model3.json".
python "%~dp0scripts\avatar_studio.py" %*
if errorlevel 1 (
  echo Studio could not finish. Review the error above.
  pause
  exit /b 1
)

@echo off
rem External viewer diagnostic; synthetic spring is not linked Physics3 evidence.
title Live2D MOC3 Real-Time Desktop Viewer (Mouse Drag Physics & States)
echo ==============================================================================
echo   Live2D MOC3 Interactive Desktop Viewer (Physics & Multi-State Engine)
echo ==============================================================================
echo   [MOUSE PHYSICS DRAG & DROP]
echo     LMB Drag on Hair/Head (Top 40%%)  : Pull hair strand & tilt head (Spring recoil)
echo     LMB Drag on Arms/Chest (Mid 30%%) : Rotate arms (Param1000) & twist torso
echo     LMB Drag on Skirt/Legs (Bot 30%%) : Pull body & sway skirt fabric
echo     Mouse Hover Anywhere              : Eyes & head track cursor in real time
echo     RMB Drag                          : Pan Camera view
echo     Mouse Wheel                       : Zoom In / Out
echo ------------------------------------------------------------------------------
echo   [STATE SWITCHING (Keys or Click Buttons at Top)]
echo     1 : Normal / Idle (Natural breathing, periodic blink, gaze follow)
echo     2 : Happy / Smile (Crescent smiling eyes, smile mouth, blush)
echo     3 : Surprised / Shock (Wide open eyes, gasp mouth, upward recoil)
echo     4 : Angry / Pout (Fierce eyebrows, pout mouth, forward tilt)
echo     5 : Shy / Blush (Full cheek blush, bashful glance down-right)
echo     6 : Dance / Physics Whip (High-energy multi-chain physics oscillation)
echo ------------------------------------------------------------------------------
echo   [ENGINE CONTROLS]
echo     Space : Toggle Auto-Motion (Breathing, sway, blink)
echo     P     : Toggle Dynamic Physics Simulation (Hair/cloth inertia)
echo     Tab   : Open/Close Manual Slider Panel
echo     M     : Toggle Offscreen Clipping Mask
echo     R     : Reset View
echo ==============================================================================
cd /d "%~dp0"
python scripts\moc3_viewer_diagnostics.py

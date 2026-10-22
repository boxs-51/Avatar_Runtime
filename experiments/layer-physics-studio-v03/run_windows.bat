@echo off
setlocal
cd /d "%~dp0"
if "%~1"=="" (
 start "Layer Physics Studio v0.3" "index.html"
 exit /b 0
)
where py >nul 2>nul
if errorlevel 1 (echo Python launcher py.exe required for .arschar preview & pause & exit /b 1)
py run_studio.py --model "%~1" --out "%~dp0generated_model_preview"
if errorlevel 1 (pause & exit /b 1)
start "Layer Physics Studio native2D preview" "%~dp0generated_model_preview\index.html"

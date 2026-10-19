@echo off
setlocal
cd /d "%~dp0"
python scripts\moc3_test_harness.py --window %*
endlocal

@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
cmake --build build-msvc-ninja --target MiniCity3D simulation_smoke audio_smoke radio_live_smoke asset_smoke -j 6

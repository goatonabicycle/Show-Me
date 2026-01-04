@echo off
echo Deploying Show Me plugins...

rmdir /s /q "C:\Program Files\Common Files\VST3\ShowMeMidi.vst3" 2>nul
rmdir /s /q "C:\Program Files\Common Files\VST3\ShowMeAudio.vst3" 2>nul
rmdir /s /q "C:\Program Files\Common Files\VST3\ShowMe.vst3" 2>nul
rmdir /s /q "C:\Program Files\Common Files\VST3\Audio.vst3" 2>nul

xcopy /E /I /Y "%~dp0Builds\VisualStudio2022\x64\Release\VST3\ShowMeMidi.vst3" "C:\Program Files\Common Files\VST3\ShowMeMidi.vst3\"
xcopy /E /I /Y "%~dp0Audio\Builds\VisualStudio2022\x64\Release\VST3\ShowMeAudio.vst3" "C:\Program Files\Common Files\VST3\ShowMeAudio.vst3\"

echo.
echo Done! Restart FL Studio and rescan plugins.
pause

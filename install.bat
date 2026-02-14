@echo off
setlocal

set "SRC=%~dp0build\SpectrogramPlugin_artefacts\Release\VST3\Spectrogram.vst3"
set "DST=C:\Program Files\Common Files\VST3\Spectrogram.vst3"

if not exist "%SRC%" (
    echo ERROR: VST3 not found. Run the build first:
    echo   cmake -B build -G "Visual Studio 17 2022" -A x64
    echo   cmake --build build --config Release --target SpectrogramPlugin_VST3
    exit /b 1
)

echo Installing Spectrogram.vst3...
robocopy "%SRC%" "%DST%" /E /IS /IT /NJH /NJS /NDL >nul
if %ERRORLEVEL% GEQ 8 (
    echo ERROR: Copy failed. Make sure Ableton is closed and run as Administrator.
    exit /b 1
)

echo Installed to %DST%
echo Close and reopen your DAW to load the new version.

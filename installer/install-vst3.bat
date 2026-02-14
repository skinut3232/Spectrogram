@echo off
echo ============================================
echo  Spectrogram VST3 Plugin Installer
echo ============================================
echo.

set "VST3_SRC=%~dp0..\build\SpectrogramPlugin_artefacts\Release\VST3\Spectrogram.vst3"
set "VST3_DST=%CommonProgramFiles%\VST3\Spectrogram.vst3"

if not exist "%VST3_SRC%" (
    echo ERROR: VST3 build not found at:
    echo   %VST3_SRC%
    echo.
    echo Please build the project first:
    echo   cmake -B build -G "Visual Studio 17 2022" -A x64
    echo   cmake --build build --config Release --target SpectrogramPlugin_VST3
    echo.
    pause
    exit /b 1
)

echo Installing VST3 to: %VST3_DST%
echo.

xcopy /E /I /Y "%VST3_SRC%" "%VST3_DST%" >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERROR: Installation failed. Try running as Administrator.
    echo   Right-click this script and select "Run as administrator"
    echo.
    pause
    exit /b 1
)

echo Installation complete!
echo.
echo The Spectrogram plugin should now appear in your DAW's VST3 plugin list.
echo You may need to rescan plugins in your DAW.
echo.
pause

; Inno Setup Script for Spectrogram VST3 Plugin
; Download Inno Setup from https://jrsoftware.org/isinfo.php

#define MyAppName "Spectrogram"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "SpectrogramAudio"

[Setup]
AppId={{E7A3B1C4-5D6F-4E8A-9B0C-1D2E3F4A5B6C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\build\installer
OutputBaseFilename=Spectrogram-{#MyAppVersion}-win64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
LicenseFile=
UninstallDisplayIcon={app}\Spectrogram.exe

[Types]
Name: "full"; Description: "Full installation (VST3 + Standalone)"
Name: "vst3only"; Description: "VST3 plugin only"
Name: "standalone"; Description: "Standalone application only"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full vst3only custom
Name: "standalone"; Description: "Standalone Application"; Types: full standalone custom

[Files]
; VST3 plugin — install to the standard VST3 folder
Source: "..\build\SpectrogramPlugin_artefacts\Release\VST3\Spectrogram.vst3\*"; DestDir: "{commoncf}\VST3\Spectrogram.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone executable
Source: "..\build\SpectrogramPlugin_artefacts\Release\Standalone\Spectrogram.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\Spectrogram.exe"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Spectrogram.exe"; Description: "Launch Spectrogram"; Flags: nowait postinstall skipifsilent; Components: standalone

[Setup]
AppName=Qenba
AppVersion=1.0.0
DefaultDirName={autopf}\Qenba
DefaultGroupName=Qenba
OutputDir=build\Output
OutputBaseFilename=QenbaSetup_v1.0.0
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "build\Release\qenba.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\SDL3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\slint_cpp.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Qenba"; Filename: "{app}\qenba.exe"
Name: "{autodesktop}\Qenba"; Filename: "{app}\qenba.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

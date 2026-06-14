[Setup]
AppName=Qenba
AppVersion=1.1.0
DefaultDirName={autopf}\Qenba
DefaultGroupName=Qenba
OutputDir=build\Output
OutputBaseFilename=QenbaSetup_v1.1.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern dark includetitlebar
SetupIconFile=config\Qenba.ico
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "build\Release\qenba.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\_deps\webview-build\core\Release\webview.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\_deps\microsoft_web_webview2-src\build\native\x64\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "third_party\slint\lib\slint_cpp.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "config\*"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Qenba"; Filename: "{app}\qenba.exe"; IconFilename: "{app}\config\Qenba.ico"
Name: "{autodesktop}\Qenba"; Filename: "{app}\qenba.exe"; IconFilename: "{app}\config\Qenba.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\qenba.exe"; Description: "{cm:LaunchProgram,Qenba}"; Flags: nowait postinstall skipifsilent

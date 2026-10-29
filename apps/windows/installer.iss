#define MyAppName "iBRA PLAYER"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "iBRA PLAYER"
#define MyAppExeName "iBRA PLAYER.exe"

[Setup]
AppId={{C1BFC2F8-5F96-4DEB-B010-2B75EF8952F1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\iBRA PLAYER
DefaultGroupName=iBRA PLAYER
OutputDir=..\..\build\windows\installer
OutputBaseFilename=iBRA-PLAYER-Setup-{#MyAppVersion}
SetupIconFile=
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "..\..\build\windows\portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\iBRA PLAYER"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\iBRA PLAYER"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch iBRA PLAYER"; Flags: nowait postinstall skipifsilent

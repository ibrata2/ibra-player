; iBRA PLAYER — Windows installer (Inno Setup 6)
; برمجة و تطوير ابراهيم الخزرجي
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\..\build\stage"
#endif
#ifndef OutDir
  #define OutDir "..\..\..\build\installer"
#endif
#define AppName "iBRA PLAYER"
#define AppExe "iBRA PLAYER.exe"

[Setup]
AppId={{C1BFC2F8-5F96-4DEB-B010-2B75EF8952F1}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Ibrahim Al-Khazraji
AppPublisherURL=https://t.me/ibraprogram
AppSupportURL=https://t.me/ibraprogram
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=iBRA PLAYER Setup {#AppVersion}
SetupIconFile=..\resources\ibra.ico
WizardImageFile=..\resources\wizard-large.bmp
WizardSmallImageFile=..\resources\wizard-small.bmp
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Per-user install by default (no administrator rights). The user may choose
; an all-users install, which then asks for elevation.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ShowLanguageDialog=yes
LicenseFile=..\..\..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
#if FileExists(CompilerPath + "Languages\Arabic.isl")
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"
#endif

[CustomMessages]
english.DesktopIcon=Create a desktop shortcut
english.ContextMenu=Add "Play with iBRA PLAYER" to the Explorer menu
english.Associations=Offer iBRA PLAYER for video and audio files (you confirm in Windows Default Apps)
english.Launch=Launch iBRA PLAYER
english.Shortcuts=Shortcuts:
english.Integration=Windows integration:
#if FileExists(CompilerPath + "Languages\Arabic.isl")
arabic.DesktopIcon=إنشاء اختصار على سطح المكتب
arabic.ContextMenu=إضافة "تشغيل باستخدام iBRA PLAYER" إلى قائمة المستكشف
arabic.Associations=اقتراح iBRA PLAYER لملفات الفيديو والصوت (تؤكد ذلك من التطبيقات الافتراضية في ويندوز)
arabic.Launch=تشغيل iBRA PLAYER
arabic.Shortcuts=الاختصارات:
arabic.Integration=التكامل مع ويندوز:
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; GroupDescription: "{cm:Shortcuts}"
Name: "contextmenu"; Description: "{cm:ContextMenu}"; GroupDescription: "{cm:Integration}"
Name: "associations"; Description: "{cm:Associations}"; GroupDescription: "{cm:Integration}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Parameters: "--register-context-menu"; Flags: runhidden runasoriginaluser waituntilterminated; Tasks: contextmenu
Filename: "{app}\{#AppExe}"; Parameters: "--register-associations"; Flags: runhidden runasoriginaluser waituntilterminated; Tasks: associations
Filename: "ms-settings:defaultapps"; Flags: shellexec runasoriginaluser nowait postinstall skipifsilent unchecked; Description: "Windows Default Apps"; Tasks: associations
Filename: "{app}\{#AppExe}"; Description: "{cm:Launch}"; Flags: nowait postinstall skipifsilent runasoriginaluser

[UninstallRun]
Filename: "{app}\{#AppExe}"; Parameters: "--unregister-all"; Flags: runhidden waituntilterminated; RunOnceId: "iBRAUnregister"

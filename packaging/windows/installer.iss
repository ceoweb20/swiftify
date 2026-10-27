#ifndef SourceRoot
  #error SourceRoot must point to the deployed application tree
#endif
#ifndef OutputRoot
  #error OutputRoot is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif

[Setup]
AppId={{0DD14A28-EFE2-4D70-9A51-7B691AC09EEA}
AppName=Swiftify
AppVersion={#AppVersion}
AppPublisher=Swiftify
AppPublisherURL=https://github.com/ceoweb20/swiftify
DefaultDirName={localappdata}\Programs\Swiftify
DefaultGroupName=Swiftify
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputRoot}
OutputBaseFilename=swiftify-{#AppVersion}-windows-x86_64-setup
SetupIconFile=..\icons\swiftify.ico
UninstallDisplayIcon={app}\bin\swiftify.exe
; OTA replaces the application tree; uninstall metadata must survive outside it.
UninstallFilesDir={localappdata}\Programs\Swiftify-uninstall
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Swiftify"; Filename: "{app}\bin\swiftify.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Swiftify"; Filename: "{app}\bin\swiftify.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\swiftify.exe"; Description: "{cm:LaunchProgram,Swiftify}"; Flags: nowait postinstall skipifsilent

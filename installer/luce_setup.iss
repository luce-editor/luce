; ============================================================================
; Luce — Inno Setup Installer Script
; ============================================================================

#define MyAppName "Luce"
#ifndef MyAppVersion
#define MyAppVersion "0.1.0"
#endif
#define MyAppPublisher "Luce Editor Team"
#define MyAppURL "https://luce-editor.github.io/luce/"
#define MyAppExeName "luce.exe"

[Setup]
AppId={{B7A58E29-4361-4DC3-8C6E-4560E58F6F9D}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
SetupIconFile=..\assets\LuceIcon.ico
UninstallDisplayIcon={app}\assets\LuceIcon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
OutputDir=..\build\installer
OutputBaseFilename=Luce-Setup-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesEnvironment=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "addtopath"; Description: "Add Luce to PATH environment variable"; GroupDescription: "Other options:"
Name: "contextmenu_file"; Description: "Add 'Open with Luce' to context menu for files"; GroupDescription: "Other options:"
Name: "contextmenu_folder"; Description: "Add 'Open with Luce' to context menu for directories"; GroupDescription: "Other options:"

[Files]
Source: "..\build\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\Release\themes\*"; DestDir: "{app}\themes"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\Release\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\assets\LuceIcon.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\assets\LuceIcon.ico"; Tasks: desktopicon

[Registry]
; Context menu for files
Root: HKA; Subkey: "Software\Classes\*\shell\OpenWithLuce"; ValueType: string; ValueData: "Open with Luce"; Flags: uninsdeletekey; Tasks: contextmenu_file
Root: HKA; Subkey: "Software\Classes\*\shell\OpenWithLuce"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\assets\LuceIcon.ico"""; Flags: uninsdeletekey; Tasks: contextmenu_file
Root: HKA; Subkey: "Software\Classes\*\shell\OpenWithLuce\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: contextmenu_file

; Context menu for folders
Root: HKA; Subkey: "Software\Classes\Directory\shell\OpenWithLuce"; ValueType: string; ValueData: "Open with Luce"; Flags: uninsdeletekey; Tasks: contextmenu_folder
Root: HKA; Subkey: "Software\Classes\Directory\shell\OpenWithLuce"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\assets\LuceIcon.ico"""; Flags: uninsdeletekey; Tasks: contextmenu_folder
Root: HKA; Subkey: "Software\Classes\Directory\shell\OpenWithLuce\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%V"""; Flags: uninsdeletekey; Tasks: contextmenu_folder

; Context menu for folder background
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\OpenWithLuce"; ValueType: string; ValueData: "Open with Luce"; Flags: uninsdeletekey; Tasks: contextmenu_folder
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\OpenWithLuce"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\assets\LuceIcon.ico"""; Flags: uninsdeletekey; Tasks: contextmenu_folder
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\OpenWithLuce\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%V"""; Flags: uninsdeletekey; Tasks: contextmenu_folder

; PATH environment variable for User (or System if elevated)
Root: HKA; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKA, 'Environment', 'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + UpperCase(Param) + ';', ';' + UpperCase(OrigPath) + ';') = 0;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  AppDir: string;
  OrigPath: string;
  P: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    AppDir := ExpandConstant('{app}');
    if RegQueryStringValue(HKA, 'Environment', 'Path', OrigPath) then
    begin
      P := Pos(';' + UpperCase(AppDir) + ';', ';' + UpperCase(OrigPath) + ';');
      if P > 0 then
      begin
        Delete(OrigPath, P - 1, Length(AppDir) + 1);
        RegWriteStringValue(HKA, 'Environment', 'Path', OrigPath);
      end;
    end;
  end;
end;

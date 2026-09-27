; 文件用途: Inno Setup 安装脚本, 把 Payload 下的便携目录打成 Windows 安装包
#ifndef AppVersion
  #define AppVersion "0.1.1"
#endif

#define AppName "DrawDesk"
#define AppPublisher "DrawDesk contributors"
#define ExeName "DrawDesk.exe"
#define PayloadDir "Payload\DrawDesk"

; 编译前确认打包内容已经准备好
#if !FileExists(AddBackslash(SourcePath) + PayloadDir + "\DrawDesk.exe")
  #error 请先运行 Scripts\Package.ps1 生成 Installer\Payload\DrawDesk, 再编译本脚本
#endif
[Setup]
AppId={{63655054-07CE-48B8-B699-C8362C170602}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=DrawDeskSetup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#ExeName}
LicenseFile={#PayloadDir}\LICENSE
SetupIconFile=..\Src\Resources\DrawDesk.ico
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} Setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#ExeName}"
Name: "{group}\恢复全部窗口"; Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -File ""{app}\RestoreAll.ps1"""; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#ExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#ExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
; CAN/UDS 车载总线诊断工具 - Inno Setup 安装脚本
; 作者：三道渊 (codeempereor)
; 版本：v1.1.0
; 日期：2026-09-02

#define MyAppName "CAN/UDS 车载总线诊断工具"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "三道渊"
#define MyAppURL "https://github.com/codeempereor/CAN-UDS-Diagnostic-Tool"
#define MyAppExeName "can_uds_tool.exe"

[Setup]
; 应用基本信息
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; 安装目录
DefaultDirName={autopf}\CAN-UDS-Diagnostic-Tool
DefaultGroupName=CAN/UDS 诊断工具
DisableProgramGroupPage=yes

; 输出设置
OutputDir=F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\installer
OutputBaseFilename=CAN_UDS_Diagnostic_Tool_v1.1.0_Setup
SetupIconFile=F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\installer\app_icon.ico
UninstallDisplayIcon=F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\installer\app_icon.ico
LicenseFile=F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\installer\LICENSE.txt
Compression=lzma2/ultra
SolidCompression=yes
WizardStyle=modern

; 权限与架构
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

; 界面设置
WizardImageStretch=no
AlwaysShowDirOnReadyPage=yes
AlwaysShowGroupOnReadyPage=yes
AllowNoIcons=yes
UninstallDisplayName={#MyAppName}

; 签名信息（可选）
; SignTool=

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 主程序和Qt运行时
Source: "F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\installer\package\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; 使用说明书
Source: "F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\installer\user_manual.md"; DestDir: "{app}\docs"; Flags: ignoreversion

; 示例DBC（已在package中，这里确保存在）
; Source: "F:\Projects\01_Embedded\CAN_UDS_Tool\CAN\examples\sample.dbc"; DestDir: "{app}"; Flags: ignoreversion

; 注意：不要在64位系统上安装32位系统文件
; 64位系统文件由系统自带，不需要打包

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\User Manual"; Filename: "{app}\docs\user_manual.md"
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即运行 {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 卸载时删除用户配置文件（可选，保留用户数据则注释掉）
; Type: filesandordirs; Name: "{app}\config"

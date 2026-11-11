; KB REVERB - BLACK ANGEL - Windows installer (Inno Setup 6)
; Build: ISCC.exe /DAppVersion=x.y.z Installer\BlackAngel.iss   (from the repository root, after a Release build)

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define PluginName "KB REVERB - BLACK ANGEL"
#define Bundle "KB REVERB - BLACK ANGEL.vst3"

[Setup]
AppId={{BE4D3598-ACFC-47E5-9568-42D6C6C8CDC6}
AppName={#PluginName}
AppVersion={#AppVersion}
AppVerName={#PluginName} {#AppVersion}
AppPublisher=KB Audio
AppPublisherURL=https://github.com/castrolabia-code/black-angel-reverb
; The uninstaller lives here; the plugin itself goes to the standard VST3 folder.
DefaultDirName={commonpf64}\KB Audio\Black Angel
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir=Output
OutputBaseFilename=KB-Reverb-Black-Angel-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#PluginName}
CloseApplications=no

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
; Remove any previous version so no stale files remain inside the bundle.
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Bundle}"

[Files]
Source: "..\build\Source\BlackAngel_artefacts\Release\VST3\{#Bundle}\*"; DestDir: "{commoncf64}\VST3\{#Bundle}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Bundle}"

[Messages]
spanish.FinishedLabel=KB REVERB - BLACK ANGEL se ha instalado en la carpeta VST3 estándar. Abre tu DAW (por ejemplo Reason) y busca el plugin en la lista de efectos. Si no aparece, pide al DAW que vuelva a buscar plugins.

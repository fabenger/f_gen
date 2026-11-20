; Inno Setup script for f_gen (built by GitHub Actions)
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define BuildDir "..\build\f_gen_artefacts\Release"

[Setup]
AppId={{6C1E2B7A-4F0D-4B5E-9C2A-F6E0A1B2C3D4}
AppName=f_gen
AppVersion={#AppVersion}
AppPublisher=fabian
DefaultDirName={autopf}\f_gen
DefaultGroupName=f_gen
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=f_gen_Setup_{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=f_gen {#AppVersion}

[Components]
Name: "vst3"; Description: "VST3 plugin (for FL Studio)"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone app (for quick testing)"; Types: full

[Files]
Source: "{#BuildDir}\VST3\f_gen.vst3\*"; DestDir: "{commoncf64}\VST3\f_gen.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\f_gen.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\f_gen (Standalone)"; Filename: "{app}\f_gen.exe"; Components: standalone
Name: "{group}\Uninstall f_gen"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\f_gen.vst3"

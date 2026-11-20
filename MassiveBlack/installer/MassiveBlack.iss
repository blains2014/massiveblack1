[Setup]
AppName=Massive Black (SnoopyChu)
AppVersion=1.0.0
DefaultDirName={commoncf64}\VST3
DisableProgramGroupPage=yes
OutputBaseFilename=MassiveBlack_Windows_Installer
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "..\build\MassiveBlack_artefacts\Release\VST3\Massive Black.vst3\*"; DestDir: "{commoncf64}\VST3\Massive Black.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

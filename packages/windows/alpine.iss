[Setup]
AppName=Alpine
#include "iss.SetupVars.tmp"
;AppVerName=Alpine 0.98
AppPublisher=University of Washington
AppPublisherURL=http://www.washington.edu/alpine/
AppSupportURL=http://www.washington.edu/alpine/faq/
AppUpdatesURL=http://www.washington.edu/alpine/getalpine/
DefaultDirName={pf}\Alpine
DefaultGroupName=Alpine
AllowNoIcons=yes
;SourceDir=E:\CreateInstall
OutputDir=..\
;OutputBaseFilename=setup_alpine_0.98
LicenseFile=legal.txt
;InfoBeforeFile=install.txt
UninstallDisplayIcon={app}\alpine.exe
;UninstallDisplayName=Alpine 0.98
UninstallFilesDir={app}\uninst_alpine
Compression=bzip/9

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "alpine.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "install.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "legal.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "ldap32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "mailcap.sam"; DestDir: "{app}"; Flags: ignoreversion
Source: "mimetype.sam"; DestDir: "{app}"; Flags: ignoreversion
Source: "pico.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "pinerc.adv"; DestDir: "{app}"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[INI]
Filename: "{app}\alpine.url"; Section: "InternetShortcut"; Key: "URL"; String: "http://www.washington.edu/alpine/"

[Icons]
Name: "{group}\Alpine"; Filename: "{app}\alpine.exe"
Name: "{group}\Pico"; Filename: "{app}\pico.exe"
Name: "{group}\Alpine on the Web"; Filename: "{app}\alpine.url"
Name: "{group}\Uninstall Alpine"; Filename: "{uninstallexe}"
Name: "{userdesktop}\Alpine"; Filename: "{app}\alpine.exe"; Tasks: desktopicon
Name: "{userdesktop}\Pico"; Filename: "{app}\pico.exe"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Alpine"; Filename: "{app}\alpine.exe"; Tasks: quicklaunchicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Pico"; Filename: "{app}\pico.exe"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\alpine.exe"; Parameters: "-nosplash -install" ; Flags: skipifsilent
Filename: "{app}\alpine.exe"; Description: "Launch Alpine"; Flags: nowait postinstall skipifsilent
Filename: "{app}\pico.exe"; Description: "Launch Pico"; Flags: nowait postinstall skipifsilent unchecked
Filename: "{app}\install.txt"; Description: "View Alpine README"; Flags: postinstall shellexec skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\alpine.url"

[UninstallRun]
Filename: "{app}\alpine.exe"; Parameters: "-nosplash -uninstall"

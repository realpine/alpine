@echo OFF
rem $Id: build.bat 14098 2005-10-03 18:54:13Z jpf@u.washington.edu $
rem ========================================================================
rem Copyright 2006 University of Washington
rem
rem Licensed under the Apache License, Version 2.0 (the "License");
rem you may not use this file except in compliance with the License.
rem You may obtain a copy of the License at
rem
rem     http://www.apache.org/licenses/LICENSE-2.0
rem
rem ========================================================================

if "%1"=="" goto blank
if "%1"=="wnt" goto wnt
if "%1"=="wnk" goto wnk
if "%1"=="w2k" goto w2k
if "%1"=="clean" goto clean
echo Unknown build command: %1 %2 %3 %4
goto usage
:blank
echo Must specify build command!
:usage
echo usage: BUILD cmd
echo   where "cmd" is one of either:
echo         WNT        -- Windows
echo         WNK        -- Windows with MIT Kerb
echo         W2K        -- Windows with Win2k Kerb
echo         CLEAN      -- to remove obj, lib, and exe files from source
goto fini

:wnt
echo PC-Pine for Windows/Winsock (Win32) build sequence
set cclntmake=makefile.nt
set alpinemake=makefile.wnt
set extracflagsnq=/Zi -Od -I../../ldap/inckit -I../ldap/inckit -D_USE_32BIT_TIME_T -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DSPCL_REMARKS=\"\\\" with SSL\\\"\"
set extralibes=
set extrarcflags="/D_PCP_WNT"
set extramakecommand=
goto build

:wnk
echo Krb5ized PC-Pine for Windows/Winsock (Win32) build sequence
set cclntmake=makefile.ntk
set alpinemake=makefile.wnt
set extracflagsnq=/I..\krb5\include /Zi -Od -I../../ldap/inckit -I../ldap/inckit -D_USE_32BIT_TIME_T -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DSPCL_REMARKS=\"\\\" with krb5 and SSL\\\"\"
set extralibes="..\krb5\lib\comerr32.lib ..\krb5\lib\gssapi32.lib ..\krb5\lib\krb5_32.lib"
set extrarcflags="/D_PCP_WNK"
set extramakecommand=
goto build

:w2k
echo Krb5ized PC-Pine for Windows/Winsock (Win32) build sequence
set cclntmake=makefile.w2k
set alpinemake=makefile.wnt
set extracflagsnq=/Zi -Od -I../../ldap/inckit -I../ldap/inckit -D_USE_32BIT_TIME_T -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DSPCFC_WINVER=\"\\\" 2000\\\"\" -DSPCL_REMARKS=\"\\\" with krb5 and SSL\\\"\"
set extralibes="secur32.lib"
set extrarcflags="/D_PCP_W2K"
set extramakecommand=
goto build

:clean
echo Sure you want to delete object, library and executable files?!?!
echo If NOT, type Ctrl-C to terminate build script NOW.  Type ENTER if you do.
pause
echo Cleaning alpine, pico, mailutil, mapi, and c-client directories
set extramakecommand=clean
set cclntmake=makefile.nt
set alpinemake=makefile.wnt
goto build

:build
set extraldflags="/DEBUG /DEBUGTYPE:CV"
set extracflags="%extracflagsnq%"
set extradllcflags="%extracflagsnq% /D_DLL"
goto buildcclnt

:buildcclnt
echo Building c-client...
cd c-client
nmake -nologo -f %cclntmake% EXTRACFLAGS=%extracflags% %extramakecommand%
if errorlevel 1 goto bogus
cd ..
goto buildmailutil

:buildmailutil
if exist mailutil goto yesbuildmailutil
goto nobuildmailutil
:yesbuildmailutil
echo Building mailutil
cd mailutil
nmake -nologo -f %cclntmake% EXTRACFLAGS=%extracflags% %extramakecommand%
if errorlevel 1 goto bogus
cd ..
:nobuildmailutil
goto buildpithosd

:buildpithosd
echo Building pith...
cd pith\osdep
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% %extramakecommand%
if errorlevel 1 goto bogus
cd ..\..
goto buildpithcc

:buildpithcc
cd pith\charconv
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% %extramakecommand%
if errorlevel 1 goto bogus
cd ..\..
goto buildpicoosd
goto buildpith

:buildpith
cd pith
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% %extramakecommand%
if errorlevel 1 goto bogus
cd ..
goto buildalpineosd
goto buildpicoosd

:buildpicoosd
echo Building pico...
cd pico\osdep
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% %extramakecommand%
if errorlevel 1 goto bogus
cd ..\..
goto buildpico

:buildpico
cd pico
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% %extramakecommand%
if errorlevel 1 goto bogus
cd ..
goto buildpith
goto buildalpineosd

:buildalpineosd
echo Building alpine...
cd alpine\osdep
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% EXTRARCFLAGS=%extrarcflags% %extramakecommand%
if errorlevel 1 goto bogus
cd ..\..
goto buildalpine

:buildalpine
cd alpine
nmake -nologo -f %alpinemake% wnt=1 EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% EXTRARCFLAGS=%extrarcflags% %extramakecommand%
if errorlevel 1 goto bogus
cd ..
goto fini
if exist c-client-dll goto buildcclntdll
goto nobuildmapi

:buildcclntdll
echo Building c-client-dll
cd c-client-dll
nmake -nologo -f %cclntmake% EXTRACFLAGS=%extradllcflags% %extramakecommand%
if errorlevel 1 goto bogus
cd ..
goto buildmapi

:buildmapi
echo Building mapi
cd mapi
nmake -nologo -f makefile EXTRACFLAGS=%extracflags% EXTRALDFLAGS=%extraldflags% EXTRALIBES=%extralibes% %extramakecommand%
if errorlevel 1 goto bogus
cd ..

:nobuildmapi
echo Alpine build complete.
goto fini

:bogus
echo Problems building Alpine!

:fini

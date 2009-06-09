@echo OFF
if "%1"=="" goto blankplat
if "%2"=="" goto blankver
if "%1"=="wnt" goto goodbuild
if "%1"=="w2k" goto goodbuild
echo Unknown build platform: %1 %2
goto usage
:blankplat
echo Must specify build platform (wnt, w2k)!
goto usage
:blankver
echo Must specify build version (eg. 0.98)
:usage
echo usage: BINSTALLER plat ver
echo   where plat is wnt or w2k
echo   and ver is version (eg 0.98)
goto fini


:goodbuild
set plat=%1
set ver=%2
set tmpfile=iss.SetupVars.tmp
echo Building installer for platform %plat% for version %ver%
echo   Assuming existence of dist.%plat%.d!
echo SourceDir=dist.%plat%.d > %tmpfile%
goto %plat%

:wnt
echo AppVerName=Alpine %ver% >> %tmpfile%
echo OutputBaseFilename=setup_alpine_%ver% >> %tmpfile%
echo UninstallDisplayName=Alpine %ver% >> %tmpfile%
goto goodbuildcont
:w2k
echo AppVerName=Alpine %ver% (with Windows Kerberos) >> %tmpfile%
echo OutputBaseFilename=setup_alpine_%ver%_w2k >> %tmpfile%
echo UninstallDisplayName=Alpine %ver% (with Windows Kerberos) >> %tmpfile%
goto goodbuildcont

:goodbuildcont
iscc alpine.iss
del %tmpfile%
echo Done.

:fini

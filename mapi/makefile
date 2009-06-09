#
# ========================================================================
# Copyright 2006 University of Washington
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================


# Nmake macros for building Windows 32-Bit apps

all: pmapi32.dll instmapi.exe

clean: 
	del *.obj *.res *.dll *.lib *.exe

CC=cl
RC=rc
LINK=link

CBASIC = -nologo -c -W3 -DWIN32 -D_WIN32
CDEBUG = #-Zi -Od
LDEBUG = #/debug /debugtype:cv
CVARSDLL = -MD -D_DLL
CVARSEXE = -MT
CFLAGS = $(CBASIC) $(CDEBUG) $(EXTRACFLAGS)
RCFLAGS = /fo pmapi.res
DLLENTRY = @12
DLLLFLAGS = -entry:_DllMainCRTStartup$(DLLENTRY) -dll $(EXTRALDFLAGS)
GUIFLAGS = -subsystem:windows
LFLAGS = $(LDEBUG) -nologo /NODEFAULTLIB
STDLIBS= ..\c-client-dll\cclient.lib winmm.lib crypt32.lib
LIBS = oldnames.lib kernel32.lib advapi32.lib ws2_32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib
LIBSDLL = msvcrt.lib $(LIBS) $(EXTRALIBES)
LIBSEXE = libcmt.lib $(LIBS)

# Update the object files if necessary

pmapi.obj: pmapi.c ..\c-client-dll\mail.h pmapi.h
	$(CC) $(CFLAGS) $(CVARSDLL) pmapi.c
smapi.obj: smapi.c ..\c-client-dll\mail.h pmapi.h
	$(CC) $(CFLAGS) $(CVARSDLL) smapi.c
rfc1522.obj: rfc1522.c ..\c-client-dll\mail.h pmapi.h
	$(CC) $(CFLAGS) $(CVARSDLL) rfc1522.c

instmapi.obj: instmapi.c
	$(CC) $(CFLAGS) instmapi.c

# Update the import library

pmapi.res: pmapi.rc
	$(RC) $(RCFLAGS) pmapi.rc

# Update the dynamic link library

pmapi32.dll: pmapi.obj smapi.obj rfc1522.obj pmapi.def pmapi.res
	..\pico\blddate > bdate.c
	$(CC) /c $(CFLAGS) bdate.c
	$(LINK) $(LFLAGS) $(DLLLFLAGS)\
	-base:0x1C000000  \
	-out:pmapi32.dll   \
	/DEF:pmapi.def   \
	pmapi.obj smapi.obj rfc1522.obj bdate.obj pmapi.res $(STDLIBS) $(LIBSDLL)

instmapi.exe: instmapi.obj
	$(LINK) $(LFLAGS) $(GUIFLAGS) -out:instmapi.exe instmapi.obj $(LIBSEXE)

distclean:

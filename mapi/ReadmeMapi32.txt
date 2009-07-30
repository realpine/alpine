ReadmeMapi32.txt
January 27, 2009
University of Washington

NOTE: It is uncertain what remains to be done for pmapi to work with
alpine.  This dll should work with a PC-Pine installation, but in
the future will be made to work with Alpine.  Registry access and
Unicode support will have to change for that to happen.

	         pmapi32 for Alpine 2.01

This distribution comes with two other files, instmapi.exe and
pmapi32.dll, for use with Aline.  It is recommended that these files
be put in your pine directory (generally C:\Program Files\Alpine).

pmapi32.dll - a Simple MAPI dynamically linked library.  pmapi32.dll
is used by other applications to access your inbox or send mail
through Alpine.

instmapi.exe - installs mapi32.dll such that registry values are set
according to the location of pmapi32.dll, and possibly copies
pmapi32.dll to the system directory if it is run on older systems.  It
will also offer to set Alpine as your default mailer and newsreader
if it is not already set.  The "-silent" option has been added to
assume that default mailer and newsreader should be set and also to
suppress a "Successful Completion" alert. This program can be run
repeatedly without side effects.

instmapi.exe must be run at least once in order for pmapi32.dll to be
accessible to other applications.  As of Alpine 4.30, instmapi.exe
may or may not need to be run in order for pmapi32.dll to work,
depending on Windows/Internet Explorer/Microsoft Office versions. To
manually set Alpine as your default mailer, you must open "Internet
Options" in your Control Panel and select the "Programs" tab.  Under
"E-mail", select Alpine.

Configuration:
Currently there is only one configuration option that is specific to
pmapi32.dll:

pmapi-send-behavior: With the ability to send messages without dialogs
it was decided that the user might want to be prompted in this case.
There are three possible values for this variable:
     always-prompt: Always prompt before sending.  This is the default
		    behavior unless set otherwise.
     always-send: Always send the message without prompting.
     never-send: Never send the message

pmapi-suppress-dialogs: MAPI Clients may tell pmapi32.dll that they
would like some dialogs allowing the user to edit the message before
sending.  Since Alpine is not yet designed to properly handle this
(nor will it be unless there is sufficient demand for it), it may be
desired to forego the Alpine interaction if the message contains
enough information for it to be set.  Possible values are:
     no: Never suppress dialogs, this is the default behavior
     yes: Always try sending it as is.  You should be aware of the
          implications before setting this
     prompt: Prompt whether or not it should fire up PC-Pine to edit
             the message

pmapi-strict-no-dialog: Some MAPI clients will request pmapi32 not to
display any dialogs, even when it is absolutely necessary that a
dialog be displayed for the purpose of logging in.  By default,
pmapi32 will just display the dialog, but this variable is available
for those who require a strict adherence of this request. The two
possible values for this variable are:
     no: Show login dialogs when necessary, which is the default
         behavior
     yes: Suppress dialogs when it is requested by the client. It
          should generally be safe to set this, as it more accurately
          reflects the specification, but it is possible that setting
          this will hide necessary authentication prompts.


To change the default setting of these variables, you will need to
either hand edit the pinerc or set a value in the Windows registry.
The pinerc setting always takes precedence over the registry setting.
An example of setting the value pmapi-send-behavior to "always-send"
in the pinerc would be to add the line:
pmapi-send-behavior=always-send
To add it to the registry, you would need to make sure the following
key exists (from the regedit program):
  HKEY_CURRENT_USER\Software\University of Washington\PC-Pine\4.0\PmapiOpts
Then, you would add a string named "pmapi-send-behavior" with the
value of "always-send"

Environment Variables:
Some sites may take advantage of environment variables in their
configurations, setting them via some program that gets executed
before running PC-Pine.  There is a way to mimmick this behavior in
MAPI.  First you would need to create the following key:
  HKEY_CURRENT_USER\Software\University of Washington\Alpine\1.0\PmapiOpts\Env
Say there is a variable that contains the string "${LOGNAME}".  To set
this for MAPI, you would add a string "LOGNAME" to the above key, and
have its value point to whatever the environment variable %LOGNAME%
would point to.

Troubleshooting: To view debugging information, create a file called
mapi_debug.txt in your system's %TEMP% directory.  Bug reports or
comments should be sent to pine@cac.washington.edu.

Jeff Franklin <jpf@washington.edu>

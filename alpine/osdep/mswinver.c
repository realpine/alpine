/*
 * Pico Versions:
 *
 * 1.0.16	Looks for 'mailcap' in the pine home directory.
 *		Slightly more informative error messages when failed to exec
 *		viewer.
 *		Changed the name to PC-Pine and PC-Pico (for Windows).
 *		Tells the user to delete the temp image file.
 * 1.0.17	Added Cut, Copy, and Paste.
 *		Several changes in pine.
 *		Fixed bug with reallocating a NULL block.
 * 1.0.18	Added "Cancel Paste!"
 * 3.90.2	Changed to use independent pine/pico version numbering.
 *		Less consistent relative to windows source code, but
 *		more consistent vis features the user sees.  Rather
 *		than mapping internal/external (public) version numbers
 *		it might make sense to keep track of internal changes by
 *		date.
 * 3.90.3	Support added for copy of whole scrolltool buffer into
 *		clipboard.  Fixed to limit amount of pasted text during
 *		text input prompts.  Windows printing added.
 *
 * ....
 *
 * 3.92, Jan 22 1995
 *		Recent modifications:  Menues, mouse sensitivity in file browser
 *		(which isn't actually seen in the windows version), and responce
 *		to MW_QUERYENDSESSION AND MW_ENDSESSION.
 *		Saved source for this version for Dr. Watson logs for tracking
 *		random protection faults.
 *
 */


#define VER_MAJOR 0
#define VER_MINOR 8
extern char datestamp[];


/*
 * Return major version number...
 */
int
mswin_majorver()
{
    return(VER_MAJOR);
}


/*
 * Return minor version number...
 */
int
mswin_minorver()
{
    return(VER_MINOR);
}


/*
 * Return compilation number...
 */
char *
mswin_compilation_date()
{
    return(datestamp);
}


/*
 * Return special remarks...
 */
char *
mswin_compilation_remarks()
{
#ifdef	SPCL_REMARKS
    return(SPCL_REMARKS);
#else
    return("");
#endif
}

/*
 * Return specific windows version...
 */
char *
mswin_specific_winver()
{
#ifdef	SPCFC_WINVER
    return(SPCFC_WINVER);
#else
    return("");
#endif
}

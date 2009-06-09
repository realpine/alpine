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
 * 2.4.1	Aligned with pico version numbers.
 * 2.4.2	Support for copy of whole text from pine's text viewer, and
 *		limited amount of text that will be pasted during text
 *		input prompts.
 * 2.4.3	Added printing capability.
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2004 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 */


#define VER_MAJOR 4
#define VER_MINOR 91
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
    return("");
}

/*
 * Return specific windows version...
 */
char *
mswin_specific_winver()
{
    return("");
}

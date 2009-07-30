/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#define VER_MAJOR 2
#define VER_MINOR 1
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

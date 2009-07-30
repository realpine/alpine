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

#define VER_MAJOR 5
#define VER_MINOR 5
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

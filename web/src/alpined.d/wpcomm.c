#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: wpcomm.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/* ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */


#include <string.h>
#include <tcl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#define	VERSION "0.1"

#define	READBUF		4096
#define RESULT_MAX	16

int	append_rbuf(char *, char *, int);
int	WPSendCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *CONST []);


/*
 * WPComm_Init - entry point for Web Alpine servlet communications.
 *
 *    returns: TCL defined values for success or failure
 *
 */

int
Wpcomm_Init(Tcl_Interp *interp)
{
    if(Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL
       && TCL_VERSION[0] == '7'
       && Tcl_PkgRequire(interp, "Tcl", "8.0", 0) == NULL)
      return TCL_ERROR;

    if(Tcl_PkgProvide(interp, "WPComm", VERSION) != TCL_OK)
      return TCL_ERROR;

    Tcl_CreateObjCommand(interp, "WPSend", WPSendCmd,
			 (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}



/*
 * WPSendCmd - establish communication with the specified device,
 *	       send the given command and return results to caller.
 *
 */
int
WPSendCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    char     buf[READBUF], lbuf[32], *errbuf = NULL, rbuf[RESULT_MAX], *fname, *cmd;
    int	     s, i, n, b, rs, rv = TCL_ERROR, wlen;
    struct   sockaddr_un name;
    Tcl_Obj *lObj;

    errno = 0;

    if(objc == 3
       && (fname = Tcl_GetStringFromObj(objv[1], NULL))
       && (cmd = Tcl_GetByteArrayFromObj(objv[2], &wlen))){
	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
	    snprintf(errbuf = buf, sizeof(buf), "WPC: socket: %s", strerror(errno));
	}
	else{
	    name.sun_family = AF_UNIX;
	    strcpy(name.sun_path, fname);

	    if(connect(s, (struct sockaddr *) &name, sizeof(name)) == -1){
		if(errno == ECONNREFUSED || errno == ENOENT)
		  snprintf(errbuf = buf, sizeof(buf), "WPC: Inactive session");
		else
		  snprintf(errbuf = buf, sizeof(buf), "WPC: connect: %s", strerror(errno));
	    }
	    else if((n = wlen) != 0){
		if(n < 0x7fffffff){
		    snprintf(lbuf, sizeof(lbuf), "%d\n", n);
		    i = strlen(lbuf);
		    if(write(s, lbuf, i) == i){
			for(i = 0; n; n = n - i)
			  if((i = write(s, cmd + i, n)) == -1){
			      snprintf(errbuf = buf, sizeof(buf), "WPC: write: %s", strerror(errno));
			      break;
			  }
		    }
		    else
		      snprintf(errbuf = buf, sizeof(buf), "Can't write command length.");
		}
		else
		  snprintf(errbuf = buf, sizeof(buf), "Command too long.");

		rbuf[0] = '\0';
		rs = 0;
		lObj = NULL;
		while((n = read(s, buf, READBUF)) > 0)
		  if(!errbuf){
		      for(i = b = 0; i < n; i++)
			if(buf[i] == '\n'){
			    if(rs){
				Tcl_AppendToObj(Tcl_GetObjResult(interp), &buf[b], i - b);
				Tcl_AppendToObj(Tcl_GetObjResult(interp), " ", 1);
			    }
			    else{
				rs = 1;
				if(append_rbuf(rbuf, &buf[b], i - b) < 0)
				  snprintf(errbuf = buf, sizeof(buf), "WPC: Response Code Overrun");
				else if(!strcasecmp(rbuf,"OK"))
				  rv = TCL_OK;
				else if(!strcasecmp(rbuf,"ERROR"))
				  rv = TCL_ERROR;
				else if(!strcasecmp(rbuf,"BREAK"))
				  rv = TCL_BREAK;
				else if(!strcasecmp(rbuf,"RETURN"))
				  rv = TCL_RETURN;
				else
				  snprintf(errbuf = buf, sizeof(buf), "WPC: Unexpected response: %s", rbuf);
			    }

			    b = i + 1;
			}

		      if(i - b > 0){
			  if(rs)
			    Tcl_AppendToObj(Tcl_GetObjResult(interp), &buf[b], i - b);
			  else if(append_rbuf(rbuf, &buf[b], i - b) < 0)
			    snprintf(errbuf = buf, sizeof(buf), "WPC: Response Code Overrun");
		      }
		  }

		if(!errbuf){
		    if(n < 0){
			snprintf(errbuf = buf, sizeof(buf), "WPC: read: %s", strerror(errno));
			rv = TCL_ERROR;
		    }
		    else if(!rs){
			if(n == 0)
			  snprintf(errbuf = buf, sizeof(buf), "WPC: Server connection closed (%d)", errno);
			else
			  snprintf(errbuf = buf, sizeof(buf), "WPC: Invalid Response to \"%.*s\" (len %d) (%d): %.*s", 12, cmd, wlen, errno, RESULT_MAX, rbuf);

			rv = TCL_ERROR;
		    }
		    else if(rv == TCL_ERROR){
			char *s = Tcl_GetStringFromObj(Tcl_GetObjResult(interp), NULL);
			if(!(s && *s))
			  snprintf(errbuf = buf, sizeof(buf), "WPC: Empty ERROR Response");
		    }
		}
	    }

	    close(s);
	}
    }
    else
      snprintf(errbuf = buf, sizeof(buf), "Usage: %s path cmd", Tcl_GetStringFromObj(objv[0], NULL));

    if(errbuf)
      Tcl_SetResult(interp, errbuf, TCL_VOLATILE);

    return(rv);
}


int
append_rbuf(char *rbuf, char *b, int l)
{
    int i;

    if(l + (i = strlen(rbuf)) > RESULT_MAX)
      return(-1);

    rbuf += i;

    while(l--)
      *rbuf++ = *b++;

    *rbuf = '\0';
    return(0);
}

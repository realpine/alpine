#!./tclsh
# $Id: do_quit.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  queryquit.tcl
#
#  Purpose:  CGI script to handle quit query, either redirecting
#            to session logout or returning to message listn
#
#  Input:
set quit_vars {
  {cid	"Command ID"}
  {quit		{} ""}
  {expinbox	{} 0}
  {expcurrent	{} 0}
  {cancel	{} ""}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

WPEval $quit_vars {
  if {$cid != [WPCmd PEInfo key]} {
    error "Invalid Command ID"
  }

  switch -regexp -- $quit {
    "^Yes, .*" {

      set exps ""

      if {[string compare $expinbox "on"] == 0} {
	append exps "&expinbox=1"
      }

      if {[string compare $expcurrent "on"] == 0} {
	append exps "&expcurrent=1"
      }

      cgi_http_head {
	WPStdHttpHdrs
      }

      cgi_html {
	cgi_head {
	  cgi_http_equiv Refresh "0; url=$_wp(serverpath)/session/logout.tcl?cid=[WPCmd PEInfo key]&sessid=${sessid}${exps}"
	}

	cgi_body {
	  cgi_table height="20%" {
	    cgi_table_row {
	      cgi_table_data {
		cgi_puts [cgi_nbspace]
	      }
	    }
	  }

	  cgi_center {
	    cgi_table border=0 width=500 cellpadding=3 {
	      cgi_table_row {
		cgi_table_data align=center rowspan=2 {
		  cgi_put [cgi_imglink logo]
		}

		cgi_table_data rowspan=2 {
		  put [nbspace]
		  put [nbspace]
		}

		cgi_table_data {
		  cgi_puts [cgi_font size=+2 face=Helvetica "Quitting Alpine ..."]
		}
	      }
	    }
	  }
	}
      }
    }
    default {
      source [WPTFScript main]
    }
  }
}

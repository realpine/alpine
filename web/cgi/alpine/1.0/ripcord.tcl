#!./tclsh
# $Id: ripcord.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  ripcord.tcl
#
#  Purpose:  CGI script to generate html page used to arm
#            short server timeout

#  Input:
set rip_vars {
  {t	""	10}
  {cid	"Command ID"}
}

#  Output:
#
#	HTML/CSS data representing the form

# inherit global config
source ./alpine.tcl

WPEval $rip_vars {

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Adjusting Session Timeout"
    }

    cgi_body BGCOLOR=#ffffff {
      if {[string compare $cid [WPCmd PEInfo key]]} {
	cgi_put "Messing around, heh?"
      } else {
	cgi_put "Making Web Alpine Server Adjustments."
	cgi_br
	cgi_put "This should only take a momment..."
	if {[catch {WPCmd PESession abandon 10}] == 0} {
	  set gonow 1
	}
      }

      cgi_script  type="text/javascript" language="JavaScript" {
	if {[info exists gonow]} {
	  cgi_puts "window.close();"
	} else {
	  cgi_puts "window.setInterval('window.close()',5000);"
	}
      }      
    }
  }
}

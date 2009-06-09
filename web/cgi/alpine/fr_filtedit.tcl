#!./tclsh
# $Id: fr_filtedit.tcl 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  fr_conf_process.tcl
#
#  Purpose:  CGI script to generate frame set for config operations
#	     in webpine-lite pages.
#            This page assumes that it was loaded by conf_process.tcl

#  Input:
set frame_vars {
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Filter Configuration"
    }

    cgi_frameset "rows=$_wp(titleheight),*" resize=yes border=0 frameborder=0 framespacing=0 {
      if {0} {
	set parms ""

	if {[info exists frame_vars]} {
	  foreach v $frame_vars {
	    if {[string length [subst $[lindex $v 0]]]} {
	      if {[string length $parms]} {
		append parms "&"
	      } else {
		append parms "?"
	      }

	      append parms "[lindex $v 0]=[subst $[lindex $v 0]]"
	    }
	  }
	}
      }

      if {[info exists filtedit_add] && $filtedit_add == 1} {
	set title 153
      } else {
	set title 154
      }
      set parms ""
      if {[info exists filtedit_add]} {
	set parms "${parms}&add=${filtedit_add}"
      }
      if {[info exists filtedit_fno]} {
	set parms "${parms}&fno=${filtedit_fno}"
      }
      if {[info exists filtedit_onfiltcancel]} {
	set parms "${parms}&onfiltcancel=${filtedit_onfiltcancel}"
      }

      cgi_frame hdr=header.tcl?title=${title}
      cgi_frame body=wp.tcl?page=filtedit&cid=$cid&oncancel=$oncancel$parms
    }
  }


#!./tclsh
# $Id: fr_cledit.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# Input:
set frame_vars {
}

# Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

cgi_http_head {
  WPStdHttpHdrs
}

  set add 0
  if {[info exists cledit_add] && $cledit_add == 1} {
    set add 1
  }
  cgi_html {
    cgi_head {
      WPStdHtmlHdr [expr {$add == 1 ? "Add Collection Configuration" : "Edit Collection Configuration"}]
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

      set title [expr {$add == 1 ? 151 : 152}]
      set parms ""
      if {[info exists cledit_add]} {
	set parms "${parms}&add=${cledit_add}"
      }
      if {[info exists cledit_cl]} {
	set parms "${parms}&cl=${cledit_cl}"
      }
      if {[info exists cledit_onclecancel]} {
	set parms "${parms}&onclecancel=${cledit_onclecancel}"
      }

      cgi_frame hdr=header.tcl?title=${title}
      cgi_frame body=wp.tcl?page=cledit&cid=$cid&oncancel=$oncancel$parms
    }
  }


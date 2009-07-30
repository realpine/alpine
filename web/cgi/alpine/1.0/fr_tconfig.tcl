#!./tclsh
# $Id: fr_tconfig.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  Intput:
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
      WPStdHtmlHdr "Configuration"
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

      set title 150

      set vlavarstr ""
      if {[info exists fr_tconfig_vlavar]} {
	set vlavarstr "&vlavar=$fr_tconfig_vlavar"
      }
      cgi_frame hdr=header.tcl?title=${title} title="Status Frame"
      cgi_frame body=wp.tcl?page=tconfig&newconf=$newconf&oncancel=$oncancel&wv=$conftype$vlavarstr title="Configuration options"
    }
  }


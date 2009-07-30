# $Id: fr_ldapquery.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_ldapquery.tcl
#
#  Purpose:  CGI script to generate frame set for LDAP server query
#	     handling in webpine-lite pages.  the idea is that this
#            page specifies a frameset that loads a "header" page 
#            used to keep the servlet alive via
#            periodic reloads and a "body" page containing static/form
#            elements that can't/needn't be periodically reloaded or
#            is blocked on user input.

#  Input:
set frame_vars {
  {dir		"Missing Directory Index"}
  {srchstr	{}	""}
  {field	{}	""}
  {op		{}	""}
  {searchtype	{}	""}
}

#  Output:
#

## read vars
foreach item $frame_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}


cgi_http_head {
  WPStdHttpHdrs
}

cgi_html {
  cgi_head {
  }

  cgi_frameset "rows=$_wp(titleheight),*" resize=yes border=0 frameborder=0 framespacing=0 {
    set parms ""

    if {[info exists frame_vars]} {
      foreach v $frame_vars {
	if {[string length [subst $[lindex $v 0]]]} {
	  if {[string length $parms]} {
	    append parms "&"
	  } else {
	    append parms "?"
	  }

	  append parms "[lindex $v 0]=[WPPercentQuote [subst $[lindex $v 0]]]"
	}
      }
    }

    cgi_frame hdr=header.tcl?title=240
    cgi_frame body=ldapquery.tcl${parms}
  }
}

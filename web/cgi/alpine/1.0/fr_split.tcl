# $Id: fr_split.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_split.tcl
#
#  Purpose:  CGI script to serve as the frame-work for including
#	     supplied script snippets that generate the various 
#	     javascript-free webpine pages

#  Input:
set split_vars {
}

#  Output:
#

## read vars
foreach item $split_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

cgi_http_head {
  WPStdHttpHdrs {} 10
}

cgi_html {
  cgi_head {
  }

  cgi_frameset "rows=33%,*" {
    set parms ""
    if {[info exists split_vars]} {
      foreach v $split_vars {
	if {[string length [subst $[lindex $v 0]]]} {
	  append parms "&[lindex $v 0]=[subst $[lindex $v 0]]"
	}
      }
    }

    cgi_frame fr_top=wp.tcl?page=index&split=1${parms} title="Message List"
    cgi_frame fr_bottom=fr_view.tcl?split=1${parms} title="Message View"
  }
}

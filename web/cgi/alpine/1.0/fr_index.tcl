#!./tclsh
# $Id: fr_index.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_index.tcl
#
#  Purpose:  CGI script to serve as the frame-work for including
#	     supplied script snippets that generate the various 
#	     javascript-free webpine pages

#  Input:
set index_vars {
  {expunge	""}
  {emptyit	""}
  {f_colid	{}	""}
  {f_name	{}	""}
  {cid		{}	0}
  {split	{}	0}
}

#  Output:
#

# read config
source ./alpine.tcl

WPEval $index_vars {
  cgi_http_head {
    WPStdHttpHdrs {} 10
  }

  cgi_html {
    cgi_head {
    }

    cgi_frameset "rows=100%,*" border=0 frameborder=0 framespacing=0 {
      set parms ""
      if {[info exists index_vars]} {
	foreach v $index_vars {
	  if {[string length [subst $[lindex $v 0]]]} {
	    append parms "&[lindex $v 0]=[subst $[lindex $v 0]]"
	  }
	}
      }

      cgi_frame body=wp.tcl?page=index${parms} title="Message List"
    }
  }
}
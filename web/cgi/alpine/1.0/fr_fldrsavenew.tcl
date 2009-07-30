# $Id: fr_fldrsavenew.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_fldrcreate.tcl
#
#  Purpose:  CGI script to generate frame set for creating a new folder to save to

#  Input:
set frame_vars {
  {onselect	""	main}
  {oncancel	""	main}
  {target	""	""}
  {controls	""	0}
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
    foreach v $frame_vars {
      if {[string length [subst $[lindex $v 0]]]} {
	append parms "&[lindex $v 0]=[WPPercentQuote [subst $[lindex $v 0]]]"
      }
    }

    switch $controls {
      2 { set tnum 223 }
      default {set tnum 222}
    }

    cgi_frame subhdr=header.tcl?title=${tnum} title="Folder Creation for Save"
    cgi_frame subbody=wp.tcl?page=fldrsavenew${parms} title="Folder Creation Frame"
  }
}

# $Id: fr_resume.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_resume.tcl
#
#  Purpose:  CGI script to generate frame set for resume composition form
#	     in webpine-lite pages.  the idea is that this
#            page specifies a frameset that loads a "header" page 
#            used to keep the servlet alive via
#            periodic reloads and a "body" page containing static/form
#            elements that can't/needn't be periodically reloaded or
#            is blocked on user input.

#  Input:
set frame_vars {
  {oncancel	""	main}
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


proc ppnd_uid {ppnd} {
  foreach i $ppnd {
    switch -- [lindex $i 0] {
      uid {
	return [lindex $i 1]
      }
    }
  }

  error "No Valid UID for Postponed message"
}


if {[catch {WPLoadCGIVar cid} result]} {
  catch {WPCmd PEInfo statmsg "Missing Command ID: $result"}
  source [WPTFScript $oncancel]
} elseif {$cid != [WPCmd PEInfo key]} {
  catch {WPCmd PEInfo statmsg "Invalid Command ID"}
  source [WPTFScript $oncancel]
} elseif {[catch {WPCmd PEPostpone count} postponed]} {
  catch {WPCmd PEInfo statmsg "Resume Error: $postponed"}
  source [WPTFScript $oncancel]
} else {
  switch -- $postponed {
    -1 -
    0 {
      catch {WPCmd PEInfo statmsg "No Postponed Messages"}
      source [WPTFScript $oncancel]
    }
    default {
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

		append parms "[lindex $v 0]=[subst $[lindex $v 0]]"
	      }
	    }
	  }

	  cgi_frame hdr=header.tcl?title=80 title="Status Frame"
	  cgi_frame body=resume.tcl${parms} title="Resumable Message List"
	}
      }
    }
  }
}

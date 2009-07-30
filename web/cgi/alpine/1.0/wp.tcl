#!./tclsh
# $Id: wp.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  wp.tcl
#
#  Purpose:  CGI script to serve as the frame-work for including
#	     supplied script snippets that generate the various 
#	     javascript-free webpine pages
#
#  Input:
set wp_vars {
  {page		{}	"main"}
  {uidList	{}	""}
  {uidpage	{}	""}
}

set wp_global_loadtime [clock clicks]

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

proc sortname {name {current 0}} {
  global rev me

  switch -- $name {
    Number { set newname "#" }
    OrderedSubj { set newname "Ordered Subject" }
    Arrival { set newname Arv }
    Status { set newname "&nbsp;" }
    default { set newname $name }
  }

  if {$current} {
    if {$rev > 0} {
      set text [cgi_imglink increas]
      set args rev=0
    } else {
      set text [cgi_imglink decreas]
      set args rev=1
    }

    append newname [cgi_url $text "wp.tcl?page=body&sortrev=1" "title=Reverse $newname ordering" target=body]
  }

  return $newname
}

proc linecolor {linenum} {
  global color

  if {$linenum % 2} {
    return $color(line1)
  } else {
    return $color(line2)
  }
}

proc lineclass {linenum} {
  if {$linenum % 2} {
    return i0
  } else {
    return i1
  }
}

proc uid_framed {u mv} {
  foreach m $mv {
    if {$u == [lindex $m 1]} {
      return 1
    }
  }
  return 0
}

WPEval $wp_vars {

  # Resolve checked uidList
  foreach uid [split $uidpage ","] {
    WPCmd PEMessage $uid select [expr [lsearch $uidList $uid] >= 0]
  }

  # sourced "page" get's CGI parms from environment
  if {[catch {WPTFScript $page} source]} {
    switch -regexp -- $page {
      addredit {
	set source fr_addredit.tcl
      }
      addrsave {
	set source addrsave.tcl
      }
      addrpick {
	set source addrpick.tcl
      }
      ldappick {
	set source ldappick.tcl
      }
      post {
	set source post.tcl
      }
      prune {
	set source prune.tcl
      }
      noop {
	cgi_html {
	  cgi_head
	  cgi_body {}
	}
	unset source
      }
      default {
	WPInfoPage "Web Alpine Error" [font size=+2 "Unknown WebPine page reference: $page."] \
	    "Please complain to the [cgi_link Admin].   Click Back button to return to previous page."
      }
    }

    if {[info exists source]} {
      set source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) $source]
    }
  }

  if {[info exists source]} {
    source $source
  }
}

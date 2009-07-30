# $Id: fr_compose.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_comp.tcl
#
#  Purpose:  CGI script to generate frame set for composer in
#	     webpine-lite pages.  the idea is that this
#            page specifies a frameset that loads a "header" page 
#            used to keep the servlet alive via
#            periodic reloads and a "body" page containing static/form
#            elements that can't/needn't be periodically reloaded or
#            is blocked on user input.

#  Input:
set frame_vars {
  {uid		""	0}
  {part		""	""}
  {style	""	""}
  {nickto       ""      ""}
  {book         ""      0}
  {ai           ""      -1}
  {notice	""	""}
  {repall	""	0}
  {reptext	""	0}
  {repqstr	""	""}
  {restore	""	0}
  {f_name	""	""}
  {f_colid	""	0}
  {cid		"Missing Command ID"}
  {spell	""	""}
  {oncancel	""	"main.tcl"}
}

#  Output:
#

## read vars
foreach item $frame_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Import Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

switch -- $style {
  Reply {
    set title "40,[WPCmd PEMessage $uid number]"
  }
  Forward {
    set title "30,[WPCmd PEMessage $uid number]"
  }
  Postponed {
    set title "20,"
  }
  Spell {
    # validate input
    set spellresult {}
    if {[catch {cgi_import_as last_line last} result] == 0 && [regexp {^[0-9]+$} $last] && $last < 10000} {
      array set replace {}

      for {set n 0} {$n < $last} {incr n} {
	if {[catch {cgi_import_as l_$n locs} result] == 0} {

	  set words {}

	  foreach l [split $locs {,}] {
	    if {[regexp {[0-9]+_([0-9]+)_([0-9]+)} $l match o len]} {
	      if {[info exists replace($l)]} {
		lappend words [list $o $len $replace($l)]
	      } elseif {([catch {cgi_import_as r_$l newword} result] == 0 && [string length [set newword [string trim $newword {  }]]])
		  || ([catch {cgi_import_as s_$l newword} result] == 0 && [string length [set newword [string trim $newword {  }]]])} {
		lappend words [list $o $len $newword]
		if {[catch {cgi_import_as a_$l allofem} result] == 0
		    && [string compare $allofem on] == 0
		    && [catch {cgi_import_as e_$l allofem} result] == 0} {
		  foreach e [split $allofem {,}] {
		    set replace($e) $newword
		  }
		}
	      }
	    }
	  }

	  if {[llength $words]} {
	    lappend spellresult [list $n $words]
	  }
	}
      }

      if {[llength $spellresult]} {
	catch {WPCmd PEInfo set wp_spellresult $spellresult}
      }
    }

    set title "10,"
  }
  default {
    set title "10,"
  }
}

cgi_http_head {
  WPStdHttpHdrs
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr Compose
  }

  cgi_frameset "rows=$_wp(titleheight),*" resize=yes border=0 frameborder=0 framespacing=0 {

    set parms ""

    if {[info exists frame_vars]} {
      foreach v $frame_vars {
	if {[string length [subst $[lindex $v 0]]]} {
	  if {[string length $parms]} {
	    append parms &
	  } else {
	    append parms ?
	  }

	  append parms "[lindex $v 0]=[WPPercentQuote [subst $[lindex $v 0]]]"
	}
      }
    }

    cgi_frame hdr=header.tcl?title=$title title="Composer Title"
    cgi_frame body=compose.tcl${parms} title="Compose Form"
  }
}

#!./tclsh
# $Id: helpbody.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  helpbody.tcl
#
#  Purpose:  CGI script to generate html help text for Web Alpine

#  Input:
set help_vars {
  {topic	""	about}
  {topicclass	""	""}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl


set sections {
  {about	0	"About Web Alpine"	about}
  {index	0	"Message List"		index}
  {view		0	"Message View"		view}
  {take		1	"Take Address"		takeaddr}
  {takeedit	1	"Take Address Edit"	takeedit}
  {folders	0	"Folder List"		folders}
  {foldiradd	1	"New Folder or Directory"	foldiradd}
  {compose	0	Compose			compose}
  {addrbrowse	1	"Address Browse"	addrbrowse}
  {attach	1	"Attach"		attach}
  {resume	0	"Resume"		resume}
  {addrbook	0	"Address Books"		addrbook}
  {addredit	1	"Edit Entry"		addredit}
}

set hidden_sections {
  {filtconf	0	"Filter Configuration"	filtconf}
  {filtedit	0	"Filter Editor"		filtedit}
}

proc WPHelpTopic {topic} {
  source genvars.tcl
  foreach g [list general_vars msglist_vars composer_vars folder_vars address_vars msgview_vars rule_vars] {
    foreach m [set $g] {
      if {[string compare $topic [lindex $m 1]] == 0} {
	return [lindex $m 2]
      }
    }
  }

  return $topic
}

WPEval $help_vars {
  foreach s $sections {
    if {[string compare $topic [lindex $s 0]] == 0} {
      set helpfile [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) help [lindex $s 3]]
    }
  }

  if {![info exists helpfile]} {
    foreach s $hidden_sections {
      if {[string compare $topic [lindex $s 0]] == 0} {
	set helpfile [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) help [lindex $s 3]]
      }
    }
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Web Alpine Help"
      WPStyleSheets
    }

    cgi_body_args
    cgi_body BGCOLOR=white "style=margin:16;margin-right:40;margin-bottom:64" {
      if {[info exists helpfile]} {
	if {[file exists ${helpfile}.tcl]} {
	  source ${helpfile}.tcl
	} elseif {[file exists ${helpfile}.html]} {

	  cgi_puts "<form action=\"[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl\" target=\"noop\" method=get>"
	  cgi_puts {<input name="page" value="noop" type=hidden notab>}

	  if {[catch {open ${helpfile}.html r} fid]} {
	    unset helpfile
	  } else {
	    cgi_put [read $fid]
	    close $fid
	  }
	} else {
	  unset helpfile
	}

	cgi_puts "</form>"
      }

      if {[info exists helpfile] == 0} {
	if {[string length $topicclass] && [catch {WPCmd PEInfo help $topic $topicclass} helptext] == 0 && [llength $helptext]} {
	  set shownvarname [WPHelpTopic $topic]

	  # rough once-over to digest it for suitable display in the alpine context
	  set show 1
	  foreach line $helptext {
	    switch -regexp -- $line {
	      {^<[/]?[hH][tT][mM][lL]>.*} -
	      {^<[/]?[tT][iI][tT][lL][eE]>.*} -
	      {^<[/]?[bB][oO][dD][yY]>.*} {
		continue
	      }
	      {^<!--chtml if pinemode=.*} {
		if {[regexp {^<!--chtml if pinemode=["]([^"]*).*} $line dummy mode]
		    && [string compare $mode web] == 0} {
		  set show 1
		} else {
		  set show 0
		}
	      }
	      {^<!--chtml else[ ]*-->} {
		set show [expr {$show == 0}]
	      }
	      {^<!--chtml endif-->} {
		set show 1
	      }
	      default {
		if {$show} {
		  set urls {}
		  while {[regexp {<[aA] ([^>]*)>} $line urlargs]} {
		    lappend urls $urlargs
		    if {[regsub {(.*)<[aA] ([^>]*)>(.*)} $line "\\1___URL___\\3" line] == 0} {
		      break 
		    }
		  }

		  # special locally expanded markup
		  set exp {<!--[#]echo var=([^ ]*)-->}
		  while {[regexp $exp $line dummy locexp]} {
		    set locexp [string trim $locexp {"}]
		    switch -regex $locexp {
		      ^FEAT_* {
			set locexp [WPHelpTopic [string range $locexp 5 end]]
		      }
		      ^VAR_* {
			set locexp [WPHelpTopic [string range $locexp 4 end]]
		      }
		    }

		    if {[regsub $exp $line $locexp line] == 0} {
		      break 
		    }
		  }

		  if {[info exists shownvarname]} {
		    regsub -all -nocase $topic $line $shownvarname line
		  }

		  foreach url $urls {
		    if {[regsub {.*[Hh][Rr][Ee][Ff]=[\"]?([^ \">]*)[\"]?.*} $url {\1} href]} {
		      set newurl "<a href=helpbody.tcl?topic=$href\\&topicclass=plain>"
		    } else {
		      set newurl ""
		    }
		    if {[regsub {(.*)___URL___(.*)} $line "\\1$newurl\\2" line] == 0} {
		      break
		    }
		  }

		  cgi_puts $line
		}
	      }
	    }
	  }
	} else {
	  cgi_center {
	    cgi_put [cgi_img [WPimg dot2] height=50]
	    cgi_put [cgi_font class=notice "Help text for [cgi_italic $topic] doesn't exist yet, but when it does it'll appear here."]
	  }
	}
      }
    }
  }
}

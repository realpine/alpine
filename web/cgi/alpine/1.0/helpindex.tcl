#!./tclsh
# $Id: helpindex.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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
#
#  helpindx.tcl
#
#  Purpose:  CGI script to generate html help text for Alpine
#
#  Input:

set help_vars {
  {topic	""	about}
  {index	""	""}
  {oncancel	""	main}
  {params	""	""}
}

#
#
#  Output:
#
#	HTML/Javascript/CSS help text for Alpine
#


# inherit global config
source ./alpine.tcl
source cmdfunc.tcl


set help_menu {
}

set sections {
  {about	0	"About Alpine"		about}
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


WPEval $help_vars {
  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Web Alpine Help"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
      lappend help_menu [list {} [list {cgi_put [cgi_nl][cgi_url [cgi_bold "Exit Help"] wp.tcl?page=${oncancel}&cid=[WPCmd PEInfo key]&x=[clock seconds] target=_top class=navbar]}]]
      lappend help_menu {}

      if {[string compare [string tolower $index] full] == 0} {
	foreach s $sections {
	  set prefix ""
	  if {[lindex $s 1]} {
	    for {set j 0} {$j < [lindex $s 1]} {incr j} {
	      append prefix [cgi_nbspace][cgi_nbspace]
	    }

	    append prefix {- }
	  }

	  if {[string compare $topic [lindex $s 0]] == 0} {
	    lappend help_menu [list {} [list "cgi_puts \"<table width=\\\"100%\\\" cellspacing=0 cellpadding=0><tr><td class=navbar bgcolor=#000000>${prefix}[lindex $s 2]</td></tr></table>\""]]
	  } else {
	    lappend help_menu [list {} [list "cgi_puts \"${prefix}\[cgi_url \"[lindex $s 2]\" \"help.tcl?topic=[lindex $s 0]&oncancel=[WPPercentQuote $oncancel]&index=${index}\" class=navbar target=body\]\""]]
	  }
	}
      } else {
	lappend help_menu [list {} [list "cgi_puts \"\[cgi_url \"About Web Alpine\" \"helpbody.tcl?topic=about&oncancel=[WPPercentQuote $oncancel]\" class=navbar target=bodtext\]\""]]
	if {[string compare [string tolower $index] none]} {
	  lappend help_menu [list {} [list "cgi_puts \"\[cgi_url \"Other Topics\" \"helpindex.tcl?topic=${topic}&index=full&oncancel=[WPPercentQuote $oncancel]\" class=navbar target=bodindx\]\""]]
	}
      }

      WPTFCommandMenu help_menu {}
    }
  }
}

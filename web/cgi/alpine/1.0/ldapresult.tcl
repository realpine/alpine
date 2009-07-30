#!./tclsh
# $Id: ldapresult.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  ldapquery.tcl
#
#  Purpose:  CGI script to submit ldap search

#  Input:
set ldap_vars {
  {dir		"Missing Directory Index"}
  {srchstr	{}	""}
  {field	{}	""}
  {op		{}	""}
  {searchtype	{}	""}
  {qn		{}	-1}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

# Command Menu definition for Message View Screen
set ldap_menu {
}

set common_menu {
  {
    {}
    {
      {
	# * * * * Ubiquitous INBOX link * * * *
	if {[string compare inbox [string tolower [WPCmd PEMailbox mailboxname]]]} {
	  cgi_put [cgi_url INBOX open.tcl?folder=INBOX&colid=0&cid=[WPCmd PEInfo key] target=_top class=navbar]
	} else {
	  cgi_put [cgi_url INBOX fr_main.tcl target=_top class=navbar]
	}
      }
    }
  }
  {
    {}
    {
      {
	# * * * * FOLDER LIST * * * *
	cgi_puts [cgi_url "Folder List" "wp.tcl?page=folders&cid=[WPCmd PEInfo key]" target=_top class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * COMPOSE * * * *
	cgi_puts [cgi_url Compose wp.tcl?page=compose&oncancel=addrbook&cid=[WPCmd PEInfo key] target=_top class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * RESUME * * * *
	cgi_puts [cgi_url Resume wp.tcl?page=resume&oncancel=addrbook&cid=[WPCmd PEInfo key] class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * Addr books * * * *
	cgi_puts [cgi_url "Address Book" wp.tcl?page=addrbook&oncancel=main.tcl target=_top class=navbar]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {}
    {
      {
	# * * * * HELP * * * *
	cgi_puts [cgi_url "Get Help" "wp.tcl?page=help&oncancel=addrbook" target=_top class=navbar]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {}
    {
      {
	# * * * * QUIT * * * *
	cgi_puts [cgi_url "Quit Web Alpine" "$_wp(serverpath)/session/logout.tcl?cid=[WPCmd PEInfo key]&sessid=$sessid class=navbar" target=_top class=navbar]
      }
    }
  }
}


WPEval $ldap_vars {

  if {[catch {WPCmd PEInfo noop} result]} {
    error [list _action "No Op" $result]
  }

  if {$qn == -1} {
    set ldapfilt ""
    set numfields 0
    if {$searchtype == 1} {
      set srchstr ""
      source ldapadvsrch.tcl
      foreach item $ldap_advanced_search {
	WPLoadCGIVarAs [lindex $item 1] tmpval
	regsub {^ *([^ ]|[^ ].*[^ ]) *$} $tmpval "\\1" tmpval
	if {$tmpval != ""} {
	  set ldapfilt "${ldapfilt}([lindex $item 2]=${tmpval})"
	  incr numfields
	}
      }
      if {$numfields > 1} {
	set ldapfilt "(&${ldapfilt})"
      }
    }
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "LDAP Query Result"
      WPStyleSheets
      cgi_puts  "<style type='text/css'>"
      cgi_puts  ".gradient	{ background-image: url('[WPimg indexhdr]') ; background-repeat: repeat-x }"
      cgi_puts  "</style>"

      if {$_wp(keybindings)} {
	set kequiv {
	  {{i} {top.location = 'fr_main.tcl'}}
	  {{l} {top.location = 'wp.tcl?page=folders'}}
	  {{?} {top.location = 'wp.tcl?page=help&oncancel=addrbook'}}
	}

	lappend kequiv

	set onload "onLoad=[WPTFKeyEquiv $kequiv]"
      } else {
	set onload ""
      }
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" $onload {

      cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	cgi_table_row {
	  #
	  # next comes the menu down the left side
	  #
	  cgi_table_data valign=top rowspan=4 class=navbar {
	    cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=2 {
	      cgi_table_row {
		cgi_table_data class=navbar style=padding-top:6 {
		  cgi_puts "Current Folder :"
		  cgi_division align=center "style=\"margin-top:4;margin-bottom:4\"" {
		    cgi_put [cgi_url [WPCmd PEMailbox mailboxname] fr_main.tcl target=_top class=navbar]
		    switch -exact -- [WPCmd PEMailbox state] {
		      readonly {
			cgi_br
			cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Read Only)"]
		      }
		      closed {
			cgi_br
			cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Closed)"]
		      }
		      ok -
		      default {}
		    }

		    cgi_br
		  }

		  cgi_hr "width=75%"
		}
	      }

	      # next comes the menu down the left side, with suitable
	      cgi_table_row {
		eval {
		  cgi_table_data $_wp(menuargs) class=navbar style=padding-bottom:10 {
		    WPTFCommandMenu ldap_menu common_menu
		  }
		}
	      }
	    }
	  }

	  cgi_table_data valign=top align=center class=dialog width=100% {

	    if {$qn == -1 && [catch {WPCmd PELdap query $dir $srchstr $ldapfilt} qn]} {
	      cgi_division align=center "style=\"background-color:white; border: 1px solid goldenrod; margin: 10; padding: 4\"" {
		cgi_puts "A problem has occured while trying to search the directory server."
		cgi_br
		cgi_br
		cgi_puts [cgi_italic [cgi_bold "$qn"]]
		cgi_br
		cgi_br
		cgi_puts "Try searching again by clicking [cgi_url "Address Book" wp.tcl?page=addrbook target=_top] at the left."
	      }
	    } elseif {$qn == 0} {
	      cgi_division align=center "style=\"background-color:white; border: 1px solid goldenrod; margin: 10; padding: 4\"" {
		cgi_puts [cgi_bold "No matches for \"$srchstr\" found."]
		cgi_br
		cgi_br
		cgi_puts "You can try another search below, or click a link at the left to continue your WebPine session."
		cgi_br
		cgi_br

		cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post enctype=multipart/form-data name=ldapsearch target=_top {
		  cgi_text "sessid=$_wp(sessid)" type=hidden notab
		  cgi_text "page=ldapquery" type=hidden notab
		  cgi_text "searchtype=0" type=hidden notab
		  cgi_text "dir=$dir" type=hidden notab

		  cgi_puts "Search Directory :"
		  cgi_text "srchstr=${srchstr}" size=35
		  cgi_submit_button "search=Search"
		}

		cgi_br
		cgi_br
	      }
	    } elseif {[catch {WPCmd PELdap results $qn} results]} {
	      cgi_division align=center "style=\"background-color:white; border: 1px solid goldenrod; margin: 10; padding: 4\"" {
		cgi_puts "A problem has occured while trying to retrieve the results of your directory search."
		cgi_br
		cgi_br
		cgi_puts [cgi_italic [cgi_bold "$results"]]
		cgi_br
		cgi_br
		cgi_puts "Try searching again by clicking [cgi_url "Address Book" wp.tcl?page=addrbook target=_top] at the left."
	      }
	    } else {
	      set numboxes 0
	      set srchindex 0
	      cgi_table border=0 cellpadding=0 cellspacing=10 width=100% {

		foreach searchres $results {
		  set srchstr [lindex $searchres 0]
		  set retdata [lindex $searchres 1]
		  set expstr ""
		  cgi_table_row {
		    cgi_table_data valign=middle align=center {
		      
		      if {$srchstr != ""} {
			set expstr " for \"[cgi_bold $srchstr]\""
		      }

		      cgi_puts [cgi_font size=+1 "Directory Search Results${expstr}"]
		    }
		  }

		  cgi_table_row {
		    cgi_table_data {
		      cgi_table border=0 bgcolor=white cellspacing=0 cellpadding=0 width=90% "style=\"border: 1px solid goldenrod; padding: 1\"" {
			set whitebg 1
			set nameindex 0
			set onetruebox 0
			set numsrchboxes 0
			foreach litem $retdata {
			  if {[llength [lindex $litem 4]] > 0} {
			    incr numsrchboxes
			    if {$numsrchboxes > 1} {
			      break
			    }
			  }
			}
			if {$numsrchboxes == 1} {
			  set onetruebox 1
			}

			cgi_table_row class=\"gradient\" {
			  cgi_table_data align=left class=indexhdr colspan=2 {
			    cgi_put "Full Name"
			  }
			  cgi_table_data align=left class=indexhdr height=30 {
			    cgi_put "Address (Click to Compose To)"
			  }
			}

			foreach litem $retdata {
			  set name [lindex $litem 0]
			  set email [lindex $litem 4]
			  set nomail 0
			  if {$whitebg == 1} {
			    set bgcolor #FFFFFF
			    set whitebg 0
			  } else {
			    set bgcolor #EEEEEE
			    set whitebg 1
			  }

			  if {[llength $email] < 1} {
			    set nomail 1
			  }

			  if {[llength $email]} {
			    set rowspan rowspan=[llength $email]
			  } else {
			    set rowspan ""
			  }

			  cgi_table_row bgcolor=$bgcolor {

			    cgi_table_data valign=top nowrap $rowspan {
			      regsub -all "<" $name "\\&lt;" name
			      regsub -all ">" $name "\\&gt;" name
			      #						    cgi_puts "$name"
			      cgi_puts "[WPurl "ldapentry.tcl?dir=${dir}&qn=${qn}&si=${srchindex}&ni=${nameindex}&email=[llength $email]" "" "$name" ""]"
			    }

			    cgi_table_data $rowspan {
			      cgi_puts [cgi_img [WPimg dot2] width=8]
			    }

			    cgi_table_data nowrap height=20px {
			      if {[llength $email] >= 1} {
				set extrarows [lrange $email 1 end]
				cgi_put [cgi_url [cgi_font size=-1 face=courier [lindex $email 0]] compose.tcl?ldap=1&dir=${dir}&qn=${qn}&si=${srchindex}&ni=${nameindex}&ei=0&cid=[WPCmd PEInfo key]&oncancel=addrbook]
			      } else {
				cgi_puts [cgi_italic "&lt;No email information&gt;"]
			      }
			    }
			  }

			  if {[info exists extrarows] && [llength $extrarows]} {
			    cgi_table_row bgcolor=$bgcolor {
			      set ei 0
			      foreach extra $extrarows {
				cgi_table_data height=20px {
				  cgi_put [cgi_url [cgi_font size=-1 face=courier $extra] compose.tcl?ldap=1&dir=${dir}&qn=${qn}&si=${srchindex}&ni=${nameindex}&ei=0&cid=[WPCmd PEInfo key]&oncancel=addrbook]
				}
			      }
			    }

			    unset extrarows
			  }

			  incr nameindex
			}
		      }
		    }
		  }
		  incr srchindex
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

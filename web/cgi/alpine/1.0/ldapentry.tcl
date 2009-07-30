#!./tclsh
# $Id: ldapentry.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  ldapentry.tcl
#
#  Purpose:  CGI script to submit ldap search

#  Input:
set ldap_vars {
  {dir		"Missing Directory Index"}
  {qn		"Missing Query Number"}
  {si		"Missing Search Index"}
  {ni		"Missing Name Index"}
  {email	""	0}
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
	# * * * * ldap Query * * * *
	cgi_puts [cgi_url "Back to Search Results" ldapresult.tcl?dir=${dir}&qn=${qn} class=navbar]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {expr {$email > 0}}
    {
      {
	# * * * * Compose To * * * *
	cgi_puts [cgi_url "Send Mail To This Person" compose.tcl?ldap=1&dir=${dir}&qn=${qn}&si=${si}&ni=${ni}&cid=[WPCmd PEInfo key]&oncancel=addrbook class=navbar]
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

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    if {$qn != 0} {
      if {[catch {WPCmd PELdap results $qn} results]} {
	WPCmd PEInfo statmsg "Some sort of ldap problem"
      }
    }

    cgi_head {
      WPStdHtmlHdr "LDAP Entry"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
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
	    if {$qn == 0} {
	      cgi_puts [cgi_italic "No matches found"]
	    } else {
	      if {[catch {WPCmd PELdap ldapext $qn "${si}.${ni}"} leinfo]} {
		cgi_br
		cgi_puts [cgi_italic "Error getting entry: $leinfo"]
	      } else {

		set lehead [lindex $leinfo 0]
		set ledata [lindex $leinfo 1]

		foreach item $ledata {
		  if {[string compare [string tolower [lindex $item 0]] name] == 0} {
		    set entry_name [lindex [lindex $item 1] 0]
		    break
		  }
		}

		cgi_division "style=\"padding:10\"" {
		  cgi_puts [cgi_font size=+1 "Directory Entry for \"$entry_name\""]
		}

		cgi_table border=0 cellspacing=0 cellpadding=0 width=80% "style=\"border: 1px solid goldenrod; padding: 2\"" {

		  set bgwhite 1
		  foreach item $ledata {
		    switch -exact -- [string tolower [lindex $item 0]] {
		      name {
			continue;
		      }
		      voicemailtelephonenumber {
			set fieldname "Voice Mail"
		      }
		      "email address" {
			set do_email 1
			set fieldname [lindex $item 0]
		      }
		      "fax telephone" {
			set do_fax 1
			set fieldname [lindex $item 0]
		      }
		      default {
			set fieldname [lindex $item 0]
		      }
		    }

		    set itematt ""
		    if {[llength $item] > 2} {
		      set itematt [lindex $item 2]
		    }
		    if {$itematt == "objectclass"} {
		      set vals [lindex $item 1]
		      continue
		    }

		    if {$bgwhite == 1} {
		      set bgcolor #ffffff
		      set bgwhite 0
		    } else {
		      set bgcolor #eeeeee
		      set bgwhite 1
		    }

		    set vals [lindex $item 1]

		    cgi_table_row bgcolor=$bgcolor {
		      cgi_table_data width=25% nowrap valign=top rowspan=[llength $vals] {
			cgi_division "style=\"padding-top:2\"" {
			  cgi_puts [cgi_bold $fieldname]
			}
		      }

		      cgi_table_data rowspan=[llength $vals] {
			cgi_puts [cgi_img [WPimg dot2] width=8]
		      }

		      cgi_table_data height=20px {
			if {[info exists do_fax]} {
			  set n {[0-9]}
			  set n3 $n$n$n
			  set n4 $n$n$n$n
			  if {[regexp "^\\\+1 ($n3) ($n3)-($n4)\$" [lindex $vals 0] dummy areacode prefix number] && [lsearch -exact {206 425} $areacode] >= 0} {
			    cgi_puts [cgi_url [lindex $vals 0] compose.tcl?ldap=1&fax=yes&dir=${dir}&qn=${qn}&si=${si}&ni=${ni}&cid=[WPCmd PEInfo key]&oncancel=addrbook]
			  } else {
			    cgi_puts [lindex $vals 0]
			  }

			  unset do_fax
			} elseif {[info exists do_email]} {
			  cgi_puts [cgi_url [cgi_font size=-1 face=courier [lindex $vals 0]] compose.tcl?ldap=1&dir=${dir}&qn=${qn}&si=${si}&ni=${ni}&ei=0&cid=[WPCmd PEInfo key]&oncancel=addrbook]
			} else {
			  cgi_puts [lindex $vals 0]
			}

			set extrarows [lrange $vals 1 end]
		      }
		    }

		    if {[info exists extrarows]} {
		      cgi_table_row bgcolor=$bgcolor {
			set ei 0
			foreach extra $extrarows {
			  cgi_table_data height=20px {
			    if {[info exists do_email]} {
			      cgi_puts [cgi_url [cgi_font size=-1 face=courier $extra] compose.tcl?ldap=1&dir=${dir}&qn=${qn}&si=${si}&ni=${ni}&ei=[incr ei]&cid=[WPCmd PEInfo key]&oncancel=addrbook]
			    } else {
			      cgi_puts $extra
			    }
			  }
			}
		      }

		      unset extrarows
		    }

		    catch {unset do_email}
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
}


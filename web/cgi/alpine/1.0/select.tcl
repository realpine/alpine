#!./tclsh
# $Id: select.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  select.tcl
#
#  Purpose:  CGI script to generate html form used to gather info
#            for message searching selection

#  Input:
set select_vars {
}

#  Output:
#
#	HTML/CSS data representing form for text select input

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

WPEval $select_vars {
  set selcount [WPCmd PEMailbox selected]

  # given a uid, called from View page so use it for defaults
  # otherwise if only one selected, use it for defaults
  set thisuid 0
  # leave disabled for now
  if {0 && $uid > 0} {
    set thisuid $uid
  } elseif {$selcount == 1} {
  }

  if {$thisuid} {
    if {[catch {WPCmd PEMessage $thisuid number} thisnum]} {
      set thisuid 0
    }
  } else {
    set thisnum ""
  }


  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Select By Text"
      WPStyleSheets
      cgi_put  "<style type='text/css'>"
      cgi_put  ".standout	{ width: 90%; text-align: center; font-size: smaller; border: 1px solid #663333; background-color: #ffcc66; padding-bottom: 8; margin-top: 8; margin-bottom: 12 }"
      cgi_puts "</style>"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" class=dialog "style=\"padding-left: 8%; padding-right: 8%\"" {

      #catch {WPCmd PEInfo set wp_index_script fr_select.tcl}
      catch {WPCmd PEInfo set help_context select}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=auth target=body {
	cgi_text "page=index" type=hidden notab
	cgi_text "doselect=1" type=hidden notab
	set mailboxname [WPCmd PEMailbox mailboxname]
	cgi_division align=center class=dialog "style=\"padding-top:6; padding-bottom:8\"" {
	  cgi_puts "This page provides a way to search for specific messages within the currently open folder, [cgi_bold $mailboxname].  Simply fill in the criteria below and click the associated [cgi_italic Search] button. All messages matching the criteria will be marked with a check in the box next to their line in the Message List.[cgi_nl][cgi_nl]"
	  cgi_puts "Click [cgi_italic Cancel] to return to the Message List without searching."
	}

	if {$selcount > 0} {
	  cgi_center {
	    cgi_division class=standout {
	      cgi_put "The folder '$mailboxname' has ${selcount} message"
	      if {[string length [WPplural $selcount]]} {
		cgi_put "s with their checkboxes marked."
	      } else {
		cgi_put " with its checkbox marked."
	      }

	      cgi_put "[cgi_nl]The Search specified below should"

	      cgi_select result {
		cgi_option "apply to entire folder, adding result to those now marked" value=broad selected
		cgi_option "apply only to marked messages, unmarking messages not matched" value=narrow
		cgi_option "discard previous marks and search anew" value=new
	      }
	    }
	  }
	} else {
	  cgi_text result=broad type=hidden notab
	}

	cgi_put "<fieldset>"
	cgi_put "<legend>[cgi_bold "Search for Text in Message Headers or Body"]</legend>"
	cgi_center {
	  cgi_put [cgi_font class=dialog "Select messages with text "]
	  cgi_select textcase {
	    cgi_option "in" value=ton
	    cgi_option "NOT in" value=not
	  }

	  cgi_br
	  cgi_br

	  cgi_put [cgi_font face=tahoma,verdana,geneva "the message's "]
	  if {$thisuid} {
	    set fromaddr [WPCmd PEMessage $thisuid fromaddr]
	    set deftext $fromaddr
	  } else {
	    if {[catch {WPCmd PEInfo set wp_def_search_text} deftext]} {
	      set deftext ""
	    }
	  }

	  set fields {
	    {Subject: field} subj ""
	    {From: field} from selected
	    {To: field} to ""
	    {Cc: field} cc ""
	    {recipient fields} recip ""
	    {participant fields} partic ""
	    {text, anywhere} any ""
	  }

	  cgi_select field {
	    foreach {x y z} $fields {
	      cgi_option $x value=$y $z
	    }
	  }

	  cgi_br
	  cgi_br
	  cgi_put [cgi_font face=tahoma,verdana,geneva "matching "]
	  cgi_text text=$deftext size=20 maxlength=256

	  if {$thisuid} {
	    set ft [WPJSQuote $fromaddr]
	    set tt [WPJSQuote [WPCmd PEMessage $thisuid toaddr]]
	    set st [WPJSQuote [WPCmd PEMessage $thisuid subject]]
	    if {[string length $ft] || [string length $tt] || [string length $st]} {
	      cgi_put "[cgi_nl]Using "
	      cgi_select defs {
		cgi_option "- Nothing -" ""
		if {[string length $ft]} {
		  cgi_option "From Address" value=$ft selected
		}
		if {[string length $tt]} {
		  cgi_option "To Address" value=$tt
		}
		if {[string length $st]} {
		  cgi_option "Subject Text" value=$st
		}
	      }
	      cgi_put " of message ${thisnum}"
	    }
	  }

	  cgi_br
	  cgi_br
	  cgi_submit_button "selectop=Search Text"
	  cgi_submit_button cancel=Cancel
	}
	cgi_put "</fieldset>"

	cgi_put "<fieldset>"
	cgi_put "<legend>[cgi_bold "Search for Messages by Date"]</legend>"
	cgi_center {
	  cgi_put "Messages dated "

	  cgi_select datecase {
	    foreach i {On Since Before} {
	      cgi_option $i value=[string tolower $i]
	    }
	  }

	  cgi_br
	  cgi_br

	  cgi_select datemon {
	    if {$thisuid} {
	      set today [string tolower [WPCmd PEMessage $thisuid date month]]
	    } else {
	      set today [string tolower [clock format [clock seconds] -format %b]]
	    }

	    set months {
	      January jan
	      February feb
	      March mar
	      April Apr
	      May may
	      June jun
	      July jul
	      August aug
	      September sep
	      October oct
	      November nov
	      December dec
	    }

	    foreach {x y} $months {
	      if {$y == $today} {
		cgi_option $x value=$y selected
	      } else {
		cgi_option $x value=$y
	      }
	    }
	  }

	  cgi_select dateday {
	    if {$thisuid} {
	      set today [WPCmd PEMessage $thisuid date day]
	    } else {
	      set today [clock format [clock seconds] -format %d]
	    }

	    for {set i 1} {$i <= 31} {incr i} {
	      set v [format "%.2d" $i]
	      if {$v == $today} {
		cgi_option $i value=$v selected
	      } else {
		cgi_option $i value=$v
	      }
	    }
	  }

	  cgi_put ",[cgi_nbspace]"
	  cgi_select dateyear {
	    if {$thisuid} {
	      set now [WPCmd PEMessage $thisuid date year]
	    } else {
	      set now [clock format [clock seconds] -format "%Y"]
	    }

	    cgi_option $now value=$now selected
	    for {set n [expr $now - 1]} {$n >= 1970} {incr n -1} {
	      cgi_option $n value=$n
	    }
	  }

	  cgi_br
	  cgi_br
	  cgi_submit_button "selectop=Search Date"
	  cgi_submit_button cancel=Cancel
	}
	cgi_put "</fieldset>"

	cgi_put "<fieldset>"
	cgi_put "<legend>[cgi_bold "Search for Messages with Certain Status Settings"]</legend>"
	cgi_center {
	  cgi_puts [cgi_font face=tahoma,verdana,geneva "Messages "]
	  cgi_select statcase {
	    cgi_option "flagged" value=ton
	    cgi_option "NOT flagged" value=not
	  }

	  cgi_puts "[cgi_font face=tahoma,verdana,geneva " :"][cgi_nl][cgi_nl]"

	  cgi_table border=0 cellpadding=2 cellspacing=0 {
	    set statuses {
	      Important imp
	      New new
	      Answered ans
	      Deleted del
	    }

	    foreach {x y} $statuses {
	      cgi_table_row {
		cgi_table_data align=right width="42%" {
		  cgi_radio_button flag=$y
		}

		cgi_table_data align=left {
		  cgi_put $x
		}
	      }
	    }
	  }

	  cgi_br
	  cgi_submit_button "selectop=Search Status"
	  cgi_submit_button cancel=Cancel
	}
	cgi_put "</fieldset>"

	cgi_center {
	  cgi_division class=standout {
	    cgi_puts "Note, if the number of messages in this folder is larger than the number of lines in the Message[cgi_nbspace]List, then some matching messages may not be visible without paging/scrolling."
	  }
	}
      }
    }
  }
}

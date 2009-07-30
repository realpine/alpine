#!./tclsh
# $Id: select2.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# coerce uid to zero since there's not method in WPL yet to initiate
# a search from a particular message.
set uid 0


# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

WPEval $select_vars {
  if {$uid} {
    if {[catch {WPCmd PEMessage $uid number} thisnum]} {
      set uid 0
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
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set wp_index_script fr_select.tcl}
      catch {WPCmd PEInfo set help_context select}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=auth target=body {
	cgi_text "page=index" type=hidden notab
	cgi_text "doselect=1" type=hidden notab
	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    cgi_table_data align=center valign=top class=dialog {
	      cgi_table width="80%" {
		cgi_table_row {
		  cgi_table_data {
		    cgi_center {
		      cgi_puts "[cgi_nl][cgi_nl]This page provides a way to search for specific messsages within the currently open folder, [cgi_bold [WPCmd PEMailbox mailboxname]].  Simply fill in the criteria below and click the assoicated [cgi_italic Search] button. All messages matching the criteria will be marked with a check in the box next to their line in the Message List.[cgi_nl][cgi_nl]"
		      cgi_puts "Click [cgi_italic Cancel] to return to the Message List without searching.[cgi_nl]"
		    }
		  }
		}

		if {[WPCmd PEMailbox selected]} {
		  cgi_table_row class=dialog {
		    cgi_table_data colspan=2 align=center valign=middle class=dialog {
		      cgi_put [cgi_font face=tahoma,verdana,geneva "Since some messages are already marked, choose whether criteria specified here should "]
		      cgi_select result {
			cgi_option "search all messages in '[WPCmd PEMailbox mailboxname]'" value=broad selected
			cgi_option "search within marked messages only." value=narrow
			cgi_option "discard previous marks and search anew." value=new
		      }

		      cgi_br
		      cgi_br
		    }
		  }
		} else {
		  cgi_text result=broad type=hidden notab
		}

		cgi_table_row class=dialog {
		  cgi_table_data {
		    cgi_table width=100% border=2 cellpadding=8 {
		      cgi_table_row {
			cgi_table_data bgcolor=#CC9900 {
			  cgi_radio_button by=text
			}

			cgi_table_data valign=top align=center nowrap class=dialog {
			  cgi_put [cgi_bold "Search for Text in Message Headers or Body"]
			  cgi_br
			  cgi_br
			  cgi_put "Select messages with text "
			  cgi_select textcase {
			    cgi_option "in" value=ton
			    cgi_option "NOT in" value=not
			  }

			  cgi_br
			  cgi_br

			  cgi_put [cgi_font face=tahoma,verdana,geneva "the message's "]
			  if {$uid} {
			    set fromaddr [WPCmd PEMessage $uid fromaddr]
			    set deftext $fromaddr
			  } else {
			    set deftext ""
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

			  if {$uid} {
			    set ft [WPJSQuote $fromaddr]
			    set tt [WPJSQuote [WPCmd PEMessage $uid toaddr]]
			    set st [WPJSQuote [WPCmd PEMessage $uid subject]]
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
			}
		      }

		      cgi_table_row class=dialog {
			cgi_table_data bgcolor=#CC9900 {
			  cgi_radio_button by=date
			}

			cgi_table_data valign=top align=center class=dialog {
			  cgi_put [cgi_bold "Search for Messages by Date"]
			  cgi_br
			  cgi_br
			  cgi_put "Messages dated "

			  cgi_select datecase {
			    foreach i {On Since Before} {
			      cgi_option $i value=[string tolower $i]
			    }
			  }

			  cgi_br
			  cgi_br

			  cgi_select datemon {
			    if {$uid} {
			      set today [string tolower [WPCmd PEMessage $uid date month]]
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
			    if {$uid} {
			      set today [WPCmd PEMessage $uid date day]
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
			    if {$uid} {
			      set now [WPCmd PEMessage $uid date year]
			    } else {
			      set now [clock format [clock seconds] -format "%Y"]
			    }

			    cgi_option $now value=$now selected
			    for {set n [expr $now - 1]} {$n >= 1970} {incr n -1} {
			      cgi_option $n value=$n
			    }
			  }
			}
		      }

		      cgi_table_row class=dialog {
			cgi_table_data bgcolor=#CC9900 {
			  cgi_radio_button by=status
			}

			cgi_table_data class=dialog align=center {
			  cgi_put [cgi_bold "Search for Messages with Certain Flag Settings"]
			  cgi_br
			  cgi_br
			  cgi_puts [cgi_font face=tahoma,verdana,geneva "Messages "]
			  cgi_select statcase {
			    cgi_option "flagged" value=ton
			    cgi_option "NOT flagged" value=not
			  }

			  cgi_puts [cgi_font face=tahoma,verdana,geneva " :"]

			  cgi_table {
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
			}
		      }
		    }
		  }
		}

		cgi_table_row class=dialog {
		  cgi_table_data valign=top align=center nowrap class=dialog {
		    cgi_br
		    cgi_submit_button ok=Search
		    cgi_submit_button cancel=Cancel
		  }
		}

		cgi_table_row class=dialog {
		  cgi_table_data colspan=2 valign=top align=center class=dialog {
		    cgi_br
		    cgi_br
		    cgi_puts [cgi_nl][cgi_nl][cgi_font size=-1 "Note, if the number of messages in this folder is larger than the number of lines in the Message List, then some matching messages may not be visible without paging/scrolling."]
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

#!./tclsh
# $Id: selstat.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  selstat.tcl
#
#  Purpose:  CGI script to generate html form used to gather info
#            for status selection

#  Input:
set select_vars {
  {uid		""			0}
}

#  Output:
#
#	HTML/CSS data representing form for status select input

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
      WPStdHtmlHdr "Search By Status"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set help_context selstat}
      catch {WPCmd PEInfo set wp_index_script fr_selstat.tcl}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=auth target=body {
	cgi_text page=index type=hidden notab
	cgi_text doselect=1 type=hidden notab
	cgi_text by=status type=hidden notab
	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    cgi_table_data align=center valign=top class=dialog {
	      cgi_table width="80%" {
		cgi_table_row {
		  cgi_table_data colspan=2 {
		    cgi_center {
		      cgi_puts "[cgi_nl][cgi_nl]This page provides a way to search for messages in [cgi_bold [WPCmd PEMailbox mailboxname]] based on their status flags. Messages matching the status selected below will be marked with a check in the box next to their line in the Message List."
		      cgi_puts "[cgi_nl][cgi_nl]Choose the status criteria below and click 'Search', or 'Cancel' to return to the Message List.[cgi_nl][cgi_nl]"
		    }
		  }

		  if {[WPCmd PEMailbox selected]} {
		    cgi_table_row class=dialog {
		      cgi_table_data colspan=2 align=center valign=middle class=dialog colspan=2 {
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
		    cgi_table_data valign=top align=center nowrap class=dialog colspan=2 {
		      cgi_puts [cgi_font face=tahoma,verdana,geneva "Search for messages "]
		      cgi_select statcase {
			cgi_option "flagged" value=ton
			cgi_option "NOT flagged" value=not
		      }

		      cgi_puts [cgi_font face=tahoma,verdana,geneva " :"]
		    }
		  }

		  set statuses {
		    Important imp
		    New new
		    Answered ans
		    Deleted del
		  }

		  if {0} {
		    cgi_table_row {
		      cgi_table_data align=center colspan=2 {
			cgi_select flag {
			  foreach {x y} $statuses {
			    cgi_option $x value=$y
			  }
			}
		      }
		    }
		  } else {
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

		  cgi_table_row {
		    cgi_table_data class=dialog align=center colspan=2 {
		      cgi_br
		      cgi_submit_button ok=Search
		      cgi_submit_button cancel=Cancel
		    }
		  }
		}

		cgi_table_row class=dialog {
		  cgi_table_data valign=top align=center class=dialog colspan=2 {
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


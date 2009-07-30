#!./tclsh
# $Id: seldate.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  seldate.tcl
#
#  Purpose:  CGI script to generate html form used to gather info
#            for date selection

#  Input:
set select_vars {
  {uid		""	0}
}

#  Output:
#
#	HTML/CSS data representing form for date select input

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
      WPStdHtmlHdr "Search By Date"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set wp_index_script fr_seldate.tcl}
      catch {WPCmd PEInfo set help_context seldate}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=auth target=body {
	cgi_text page=index type=hidden notab
	cgi_text doselect=1 type=hidden notab
	cgi_text by=date type=hidden notab
	if {![WPCmd PEMailbox selected]} {
	  cgi_text result=broad type=hidden notab
	}

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    cgi_table_data align=center valign=top class=dialog {
	      cgi_table width="80%" {
		cgi_table_row {
		  cgi_table_data {
		    cgi_center {
		      cgi_puts "[cgi_nl]This page provides a way to search for messages in [cgi_bold [WPCmd PEMailbox mailboxname]] based on arrival time."
		      cgi_puts "[cgi_nl][cgi_nl]Messages arriving [cgi_italic On] the date entered below will be marked with a check in the box next to their line in the Message List.  Choosing [cgi_italic Since] marks messages arriving between today and the giving date (including the given date).  Choosing [cgi_italic Before] marks messages arriving before (but not on) the given date."
		      cgi_puts "[cgi_nl][cgi_nl]Choose a date below and click 'Search' to choose messages, or 'Cancel' to return to the Message List.[cgi_nl][cgi_nl]"
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
		      cgi_br
		    }
		  }
		}
		
		cgi_table_row class=dialog {
		  cgi_table_data valign=top align=center class=dialog {
		    cgi_put [cgi_font face=tahoma,verdana,geneva "Messages dated "]

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

		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_br
		    cgi_submit_button ok=Search
		    cgi_submit_button cancel=Cancel
		  }
		}

		cgi_table_row class=dialog {
		  cgi_table_data valign=top align=center class=dialog {
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

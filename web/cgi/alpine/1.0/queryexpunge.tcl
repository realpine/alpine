#!./tclsh
# $Id: queryexpunge.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  queryexpunge.tcl
#
#  Purpose:  CGI script to generate html form used to confirm 
#            deleted message expunge

#  Input (Assumed set by sourcing script):
#     fn           : Name of folder getting expunged
#     delcount     : Number of deleted messages

#  Output:
#
#	HTML/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl

WPEval {} {

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Confirm Expunge"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set help_context expunge}

      cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	cgi_table_row {
	  cgi_table_data valign=top align=center class=dialog {
	    cgi_form $_wp(appdir)/$_wp(ui1dir)/fr_index method=get name=confirm target=spec {
	      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab

	      set mbn [WPCmd PEMailbox mailboxname]
	      cgi_table border=0 cellspacing=8 cellpadding=8 width="75%" {

		if {[catch {WPCmd PEMailbox flagcount deleted} delcount] == 0 && $delcount > 0
		    && [catch {WPCmd PEMailbox messagecount} messcount] == 0} {

		  cgi_table_row {
		    cgi_table_data align=center valign=middle height=50 {
		      cgi_table bgcolor=yellow background=[WPimg dstripe] cellpadding=6 {
			cgi_table_row {
			  cgi_table_data {
			    cgi_table bgcolor=black cellpadding=6 {
			      cgi_table_row {
				cgi_table_data {
				  cgi_puts [cgi_font size=+2 color=yellow [cgi_bold "CAUTION!"]]
				}
			      }
			    }
			  }
			}
		      }
		    }
		  }

		  if {$delcount == $messcount} {
		    switch $delcount {
		      1 {
			set m1 "The [cgi_bold only] message in the folder [cgi_bold $mbn] is deleted."
			set m2 "that [cgi_bold single] message"
		      }
		      2 {
			set m1 "[cgi_bold Both] messages in the folder [cgi_bold $mbn] are deleted."
			set m2 "[cgi_bold "both"] messages"
		      }
		      default {
			set m1 "[cgi_span "style=font-weight: bold; color: red" "All $messcount messages"] in the folder [cgi_bold $mbn] are marked for deletion.  This includes any messages that might <u>not</u> be <u>visible</u> on the screen."
			set m2 "[cgi_bold [cgi_span "style=font-size: bigger; color: red; text-decoration: underline" "all messages"]]"
		      }
		    }

		    append m1 "[cgi_nl][cgi_nl]Expunge now will leave this folder "
		    append m1 "[cgi_span "style=font-weight: bold; color: red" empty]. "
		    #append m1 "[cgi_nl][cgi_nl]Please acknowledge below that you understand there will be [cgi_span "style=font-weight: bold; color: red" "no more messages"] within this folder when the expunge is complete."
		    append m1 "[cgi_nl][cgi_nl][cgi_buffer {cgi_checkbox "emptyit=1"}] "
		    append m1 "I acknowledge expunge will leave folder [cgi_bold $mbn] [cgi_span "style=font-weight: bold; color: red" empty]."
		    set style "style=\"border: 1px solid #663333; background-color: #ffcc66;\""
		    set m3 ALL
		  } else {
		    if {$delcount > 1} {
		      set whch are
		      set plrl "s"
		    } else {
		      set whch is
		      set plrl ""
		    }

		    set m1 "Your folder [cgi_bold $mbn] contains $messcount messages, of which [cgi_bold $delcount] $whch deleted."
		    set m2 "[cgi_span "style=font-size: bigger; font-weight: bold" $delcount] message${plrl}"
		    set m3 $delcount
		    set style ""
		  }

		  cgi_table_row {
		    cgi_table_data align=center $style {
		      cgi_puts $m1
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_puts "Do you wish to [cgi_span "style=color: red ; font-weight: bold" "permanently remove"] $m2 now?"
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_submit_button "expunge=Yes, Remove $m3 message[WPplural $delcount]" tabindex=2
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_submit_button "cancel=No, Return to '$mbn'" tabindex=1 checked selected default
		    }
		  }
		} else {
		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_puts "There are [cgi_bold no] messages currently marked for deletion in the folder [cgi_bold [WPCmd PEMailbox mailboxname]]."
		    }
		  }

		  if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
		    switch [WPCmd PEInfo aggtabstate] {
		      0 {
			lappend methods "Click the [cgi_img [WPimg slideout] style=vertical-align:middle] tab to expose aggregate operations"
			lappend methods "Place a mark in the checkbox next to each desired message"
			lappend methods "Click the [cgi_italic Delete] button"
		      }
		      1 {
			lappend methods "Place a mark in the checkbox next to each desired message"
			lappend methods "Click the [cgi_italic Delete] button"
		      }
		      2 {
			lappend methods "Place a mark in the checkbox next to each desired message"
			lappend methods "Within the Message Status box, choose [cgi_bold Deleted] from the drop-down list of flag choices"
			lappend methods "Click the [cgi_italic Set] button"
		      }
		    }

		    cgi_table_row {
		      cgi_table_data align=center {
			cgi_puts "To mark a message for deletion while viewing it, simply click the [cgi_italic Delete] button at the top of the Message View page."
		      }
		    }

		    cgi_table_row {
		      cgi_table_data align=center {
			cgi_puts "To mark messages for deletion in the Message List:"
			cgi_number_list {
			  foreach i $methods {
			    cgi_li $i
			  }
			}
		      }
		    }
		  } else {
		    cgi_table_row {
		      cgi_table_data align=center {
			cgi_puts "To mark a message for deletion, click the [cgi_italic Delete] button while viewing the message."
		      }
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_puts "Click [cgi_italic OK] to return to the Message List."
		      cgi_br
		      cgi_br
		      cgi_submit_button cancel=OK
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
}

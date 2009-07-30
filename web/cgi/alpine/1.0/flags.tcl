#!./tclsh
# $Id: flags.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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
#
#  Input:
set flag_vars {
  {uid	"Missing UID"}
}

#  Output:
#
#	HTML/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl

WPEval $flag_vars {

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Set Flags"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=confirm target=body {
	cgi_text "page=index" type=hidden notab
	cgi_text "sessid=$sessid" type=hidden notab
	cgi_text "uid=$uid" type=hidden notab

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    cgi_table_data valign=top align=center class=dialog {
	      cgi_table border=0 cellspacing=0 cellpadding=2 width="75%" {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts [cgi_nl][cgi_nl]
		    cgi_puts "Status flags are attributes you can assign to messages that help you associate certain meanings (for example, [cgi_bold Important]) or indicate to Web Alpine"
		    cgi_puts "how you would like to operate on the message (for example, the [cgi_bold Deleted] flag tells WebPine to permanently remove the"
		    cgi_puts "message from the folder when you [cgi_bold Expunge])."
		    cgi_puts [cgi_nl][cgi_nl]
		    cgi_puts "Set or unset desired flags for message [WPCmd PEMessage $uid number] below then"
		    cgi_puts "click [cgi_italic "Set Flags"],  or [cgi_italic Cancel] to return to the list of messages"
		    cgi_puts "in [WPCmd PEMailbox mailboxname]."
		    cgi_br
		    cgi_br

		    set flaglist [WPCmd PEInfo flaglist]
		    set setflags [WPCmd PEMessage $uid status]

		    cgi_table class=dialog {
		      foreach item $flaglist {
			cgi_table_row {
			  cgi_table_data valign=top align=right width="30%" {
			    if {[lsearch $setflags $item] >= 0} {
			      set checked checked
			    } else {
			      set checked ""
			    }

			    cgi_checkbox $item style="background-color:$_wp(dialogcolor)" $checked
			  }

			  cgi_table_data valign=top align=left {
			    switch -- $item {
			      New	{ set text "New" }
			      Answered { set text "Answered" }
			      Deleted { set text "Deleted" }
			      default { set text $item }
			    }
			    cgi_puts $text
			  }
			}
		      }

		      cgi_table_row {
			cgi_table_data colspan=2 height=50 {
			  cgi_br
			  cgi_submit_button "op=Set Flags"
			  cgi_submit_button cancel=Cancel
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
  }
}

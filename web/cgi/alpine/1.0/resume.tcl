#!./tclsh
# $Id: resume.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  resume.tcl
#
#  Purpose:  CGI script to browse postponed messages

#  Input:
set resume_vars {
  {oncancel "" main.tcl}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl


set resume_cmds {
  {
    {}
    {
      {
	cgi_puts [cgi_url "Get Help" wp.tcl?page=help&oncancel=resume target=_top class=navbar]
      }
    }
  }
}

WPEval $resume_vars {

  if {![info exists oncancel]} {
    WPLoadCGIVarAs oncancel oncancel
  }

  if {[catch {WPCmd PEPostpone list} postponed]} {
    error [list _action Resume $postponed "Click Back button to try again."]
  }

  set charset [lindex $postponed 1]

  cgi_http_head {
    WPStdHttpHdrs "text/html; charset=\"$charset\""
  }

  cgi_html {
    cgi_head {
      cgi_http_equiv Content-Type "text/html; charset=$charset"
      WPStdHtmlHdr "Postponed Messages"
      WPStyleSheets
      cgi_puts  "<style type='text/css'>"
      cgi_puts  ".gradient	{ background-image: url('[WPimg indexhdr]') ; background-repeat: repeat-x }"
      cgi_puts  "</style>"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set help_context resume}

      cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {
	cgi_table_row {
	  # next comes the menu down the left side
	  eval {
	    cgi_table_data $_wp(menuargs) {
	      WPTFCommandMenu resume_cmds {}
	    }
	  }

	  cgi_table_data width=100% valign=top class=dialog {

	    cgi_table border=0 cellspacing=0 cellpadding=0 width=100% {

	      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post target=_top {
		cgi_text "page=compose" type=hidden notab
		cgi_text "style=Postponed" type=hidden notab
		cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
		cgi_text "oncancel=$oncancel" type=hidden notab


		set n 1
		set fmt {{to Recipient} {date Date} {subj Subject}}
		cgi_table_row class=\"gradient\" {
		  cgi_table_data class=indexhdr height=$_wp(indexheight) {
		    cgi_puts [cgi_nbspace]
		  }

		  foreach i $fmt {
		    cgi_table_data class=indexhdr {
		      cgi_puts [lindex $i 1]
		    }
		  }
		}

		set checked ""
		set last [llength [lindex $postponed 0]]
		for {set i 0} {$i < $last} {incr i} {

		  #	cgi_html_comment [lindex [lindex $postponed 0] $i]
		  array set pa [join [lindex [lindex $postponed 0] $i]]

		  if {[info exists pa(uid)] == 0} {
		    continue;
		  }

		  if {$i % 2} {
		    set bgcolor #EEEEEE
		  } else {
		    set bgcolor #FFFFFF
		  }

		  if {[expr $i + 1] == $last} {
		    set checked checked
		  }

		  cgi_table_row bgcolor=$bgcolor {
		    cgi_table_data valign=top nowrap bgcolor=$bgcolor {
		      cgi_radio_button "uid=$pa(uid)" style="background-color:$bgcolor" $checked
		    }

		    foreach j $fmt {
		      cgi_table_data bgcolor=$bgcolor {
			if {[info exists pa([lindex $j 0])]} {
			  cgi_puts $pa([lindex $j 0])
			} else {
			  cgi_puts [cgi_nbspace]
			}
		      }
		    }
		  }

		  set checked ""
		}

		cgi_table_row {
		  cgi_table_data align=center colspan=24 {
		    cgi_br
		    cgi_submit_button "resume=Resume Chosen Message"
		  }
		}
	      }

	      cgi_table_row {
		cgi_table_data align=center colspan=24 {
		  cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=_top {
		    cgi_text "page=$oncancel" type=hidden notab
		    cgi_br
		    cgi_submit_button "cancel=Cancel"
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

#!./tclsh
# $Id: queryattach.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  queryattach.tcl
#
#  Purpose:  CGI script to generate html form used to ask for 
#            attachment to composition

#  Input:

#  Output:
#
#	HTML/CSS data representing the form

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

# make sure form's in Unicode
set charset "UTF-8"

set query_menu {
  {
    {}
    {
      {
	cgi_puts "Get Help"
      }
    }
  }
  {
    {expr 0}
    {
      {
	# * * * * OK * * * *
	cgi_submit_button "attach=Add Attachment" class="navbar"
      }
    }
  }
  {
    {expr 0}
    {
      {
	# * * * * CANCEL * * * *
	cgi_submit_button cancel=Cancel class="navbar"
      }
    }
  }
  {
    {expr 0}
    {
      {
	# * * * * Address/Cancel * * * *
	cgi_submit_button doit=Done class="navbar"
	cgi_br
	cgi_select attachop class=navtext {
	  cgi_option "Action..." value=null
	  cgi_option Attach value=attach
	  cgi_option Cancel value=cancel
	}
      }
    }
  }
}

WPEval {} {

  cgi_http_head {
    WPStdHttpHdrs "text/html; charset=\"$charset\""
  }

  cgi_html {
    cgi_head {
      cgi_http_equiv Content-Type "text/html; charset=$charset"
      WPStdHtmlHdr "Attach"
      WPStyleSheets
      cgi_put  "<style type='text/css'>"
      cgi_put  ".filename	{ font-family: Courier, monospace ; font-size: 10pt }"
      cgi_puts "</style>"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post enctype=multipart/form-data target=_top {
	cgi_text page=attach type=hidden notab
	cgi_text cid=[WPCmd PEInfo key] type=hidden notab
	if {[info exists params]} {
	  foreach p $params {
	    cgi_text "[lindex $p 0]=[lindex $p 1]" type=hidden notab
	  }
	}

	cgi_table border=0 cellpadding=0 cellspacing=0 width="100%" height="100%" {
	  cgi_table_row {
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu query_menu {}
	      }
	    }

	    cgi_table_data align=center valign=top class=dialog {
	      cgi_table border=0 width=75% cellpadding=15 {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts "To attach a file to your message, enter its path and file name below, or use the [cgi_italic Browse] button to choose the file, then click [cgi_italic "Add Attachment"], or click [cgi_italic Cancel] to return to your composition without attaching anything."
		  }
		}
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_file_button file "accept=*/*" size=30 class=filename
		  }
		}
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts "You can also provide a short description to help the message's recipient figure out what the attachment is :"
		  }
		}
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_text description= maxlength=256 size=40 class=filename
		  }
		}
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_submit_button "attach=Add Attachment"
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

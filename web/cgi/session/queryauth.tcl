#!./tclsh
# $Id: queryauth.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  queryauth.tcl
#
#  Purpose:  CGI script to generate html form used to ask for authentication
#            credentials

# input:
set query_vars {
  {cid		"Missing Command ID"}
  {authcol	"Missing Authenticaion Collection"}
  {authfolder	"Missing Authentication Folder"}
  {authpage	"No Post Authorization Instructions"}
  {authcancel	"No Auth Cancel Instructions"}
  {authuser	""	""}
  {reason	""	""}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument


# inherit global config
source ./alpine.tcl
source ../$_wp(appdir)/$_wp(ui1dir)/cmdfunc.tcl


set query_menu {
  {
    {}
    {
      {
	# * * * * Help * * * *
	cgi_put "Get Help"
      }
    }
  }
}


WPEval $query_vars {

  if {$cid != [WPCmd PEInfo key]} {
    error [list _action open "Invalid Operation ID" "Click Back button to try again."]
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Authentication Credentials"
      WPStyleSheets
    }

    if {[string length $authuser]} {
      set onload "onLoad=document.auth.pass.focus()"
    } else {
      set onload "onLoad=document.auth.user.focus()"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" $onload {
      cgi_form $_wp(serverpath)/session/setauth.tcl method=post enctype=multipart/form-data name=auth target=_top {
	cgi_text "sessid=$sessid" type=hidden notab
	cgi_text "cid=$cid" type=hidden notab
	cgi_text "authcol=$authcol" type=hidden notab
	cgi_text "authfolder=$authfolder" type=hidden notab
	cgi_text "authpage=$authpage" type=hidden notab
	cgi_text "authcancel=$authcancel" type=hidden notab

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu query_menu {}
	      }
	    }

	    cgi_table_data valign=top class=dialog {
	      cgi_division align=center class=dialog "style=\"padding:30 12%\"" {
		if {[info exists reason] && [string compare BADPASSWD [string range $reason 0 8]]} {
		  cgi_puts $reason
		} else {
		  cgi_puts "Login Required"
		}
	      }

	      cgi_center {
		cgi_puts [cgi_font size=+1 class=dialog "Username: "]
		cgi_text user=$authuser maxlength=30 size=25%
		cgi_br
		cgi_br
		cgi_puts [cgi_font size=+1 class=dialog "Password: "]
		cgi_text pass= type=password maxlength=30 size=25%
		cgi_br
		cgi_br
		cgi_submit_button auths=Login
		cgi_submit_button cancel=Cancel
	      }
	    }
	  }
	}
      }
    }
  }
}

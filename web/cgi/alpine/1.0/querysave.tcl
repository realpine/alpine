#!./tclsh
# $Id: querysave.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  querysave.tcl
#
#  Purpose:  CGI script to generate html form used to gather folder
#            name and collection for aggregate save

#  Input:
#     conftext : 
#     params : array of key/value pairs to submit with form
#     oncancel : url to reference should user cancel dialog
set qsave_vars {
}

#  Output:
#
#	HTML/CSS data representing the form for save folder dialog

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set query_menu {
  {
    {}
    {
      {
	# * * * * OK * * * *
	cgi_image_button save=[WPimg but_save] border=0 alt="Save"
      }
    }
  }
  {
    {}
    {
      {
	# * * * * CANCEL * * * *
	cgi_puts [cgi_url [cgi_img [WPimg but_cancel] border=0 alt="Cancel"] wp.tcl?${oncancel}]
      }
    }
  }
}

WPEval $qsave_vars {
  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Aggregate Save"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get {
	if {[info exists params]} {
	  foreach p $params {
	    cgi_text "[lindex $p 0]=[lindex $p 1]" type=hidden notab
	  }
	}

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    cgi_table_data valign=top align=center class=dialog {
	      cgi_text "page=selsave" type=hidden notab
	      cgi_text "by=text" type=hidden notab
	      cgi_text "postpage=index" type=hidden notab

	      cgi_puts [cgi_nl][cgi_nl][cgi_nl][cgi_nl]
	      cgi_puts "You are attempting to Save to a folder, '$folder', that does not exist."
	      cgi_br
	      cgi_puts "[cgi_nl]Click 'Create' to create the folder and save the message, or 'Cancel' to abort the save."
	    }
	    cgi_table_row {
	      cgi_table_data {
		cgi_submit_button save=Save
		cgi_submit_button cancel=Cancel
	      }
	    }
	  }
	}
      }
    }
  }

}

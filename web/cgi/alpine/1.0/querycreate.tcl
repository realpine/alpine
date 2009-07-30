#!./tclsh
# $Id: querycreate.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  querycreate.tcl
#
#  Purpose:  CGI script to generate html form used to confirm folder
#            creation for Save

#  Input:
#     conftext : 
#     params : array of key/value pairs to submit with form
#     oncancel : url to reference should user cancel confirmation
set qcreate_vars {
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set query_menu {
  {
    {}
    {
      {
	# * * * * OK * * * *
	cgi_image_button create=[WPimg but_create] border=0 alt="Create"
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

WPEval $qcreate_vars {

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Confirm Creation"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      if {[catch {WPCmd PEInfo set querycreate_state} qstate]} {
	
      } else {
	catch {WPCmd PEInfo unset querycreate_state}

	set folder [lindex $qstate 0]
	set params [lindex $qstate 1]

	catch {WPCmd PEInfo set help_context create_save}

	cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=confirm target=spec {
	  if {[info exists params]} {
	    foreach p $params {
	      cgi_text "[lindex $p 0]=[lindex $p 1]" type=hidden notab
	    }
	  }

	  cgi_table border=0 cellspacing=0 cellpadding=30 width="100%" height="100%" class=dialog {
	    cgi_table_row {
	      cgi_table_data align=center valign=top {
		cgi_table width="80%" border=0 {
		  cgi_table_row {
		    cgi_table_data valign=top align=center {
		      cgi_puts "You are attempting to Save to a folder, [cgi_bold $folder], that does not exist."
		      cgi_br
		      cgi_puts "[cgi_nl]Click [cgi_italic Create] to create [cgi_bold $folder] and save the message, or [cgi_italic Cancel] to create nothing and return to the Message View."
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_br
		      cgi_submit_button create=Create
		      cgi_submit_button savecancel=Cancel
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

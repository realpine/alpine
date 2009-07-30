#!./tclsh
# $Id: querynewdir.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  querynewfldr.tcl
#
#  Purpose:  CGI script to generate html form used to confirm 
#            folder creation

#  Input:
set fldr_vars {
  {fid	"No Collection Specified"}
}

#  Output:
#
#	HTML/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set query_menu {
  {
    {}
    {
      {
	# * * * * HELP * * * *
	cgi_put "Get Help"
      }
    }
  }
}

WPEval $fldr_vars {

  if {[catch {WPCmd PEFolder collections} collections]} {
    error [list _action "Collection list" $collections]
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Folder Creation"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set help_context diradd}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=confirm target=_top {
	cgi_text "page=folders" type=hidden notab
	cgi_text "cid=[WPCmd PEInfo key]" type=hidden
	cgi_text "fid=$fid" type=hidden
	cgi_text "frestore=1" type=hidden

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu query_menu {}
	      }
	    }
	    
	    cgi_table_data valign=top align=center class=dialog {
	      cgi_table border=0 cellspacing=0 cellpadding=2 width="70%" {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts [cgi_nl][cgi_nl][cgi_nl][cgi_nl]
		    cgi_puts "Please enter the name of the directory you would like to add"
		    if {[llength $fid] > 1} {
		      cgi_put " to the directory '[join [lrange $fid 1 end] /]'"
		    }
		    if {[llength $collections] > 1} {
		      cgi_put "in the collection '[lindex [lindex $collections [lindex $fid 0]] 1]'."
		    } else {
		      cgi_put "."
		    }
		    cgi_br
		    cgi_br
		    cgi_put "New directory name: "
		    cgi_text directory= maxlength=64 size=25%
		    cgi_br
		    cgi_br
		    cgi_puts "Click 'Create' to add a new directory by that name, or 'Cancel' to return to the Folder List."
		    cgi_br
		    cgi_br
		    cgi_submit_button newdir=Create
		    cgi_submit_button newdir=Cancel
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

#!./tclsh
# $Id: querynewfoldir.tcl 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  querynewfoldir.tcl
#
#  Purpose:  CGI script to generate html form used to confirm 
#            folder and directory creation
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
	cgi_put [cgi_url "Get Help" wp.tcl?page=help&oncancel=folders class=navbar target=_top]
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

      catch {WPCmd PEInfo set help_context foldiradd}

      cgi_form $_wp(appdir)/wp method=get name=confirm target=_top {
	cgi_text "page=folders" type=hidden notab
	cgi_text "fid=$fid" type=hidden
	cgi_text "cid=[WPCmd PEInfo key]" type=hidden
	cgi_text "frestore=1" type=hidden

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu query_menu {}
	      }
	    }

	    if {[llength $fid] > 1} {
		set Dirpref "Subd"
		set dirpref "subd"
	    } else {
		set Dirpref "D"
		set dirpref "d"
	    }
	    
	    cgi_table_data valign=top align=center class=dialog {
	      cgi_table border=0 cellspacing=0 cellpadding=2 width="70%" {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts [cgi_nl][cgi_nl]
		      cgi_put "This page provides a way for you to create a new folder or ${dirpref}irectory"
		      if {[llength $fid] > 1} {
			cgi_put " within the directory [cgi_bold [join [lrange $fid 1 end] /]]"
		      }
		      if {[llength $collections] > 1} {
			cgi_put " in the collection [cgi_bold [lindex [lindex $collections [lindex $fid 0]] 1]]."
		      } else {
			cgi_put "."
		      }
		  }
		}
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_br
		    cgi_put "Folders are used to contain messages.  Typically, messages are placed in folders when you [cgi_italic Save] them from the Message View"
		    if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
		      cgi_puts " or Message List pages."
		    } else {
		      cgi_puts "page."
		    }

		    cgi_put "To create a new folder, enter the name below and click [cgi_italic "Create New Folder"]."
		    cgi_br
		    cgi_br
		    cgi_put "New folder name: "
		    cgi_text folder= maxlength=64 size=25%
		    cgi_br
		    cgi_br
		    #cgi_puts "Once you have entered the desired folder name above, click below to create the new folder, or 'Cancel' to return to the Folder List."
		    #cgi_br
		    cgi_br
		    cgi_submit_button "newfolder=Create New Folder"
		    cgi_submit_button cancelled=Cancel
		  }
		}

		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_br
		    cgi_br
		    cgi_puts "${Dirpref}irectories are used to contain folders.  It is sometimes helpful to organize folders containing"
		    cgi_puts "messages sharing common topics or themes in a ${dirpref}irectory."
		    cgi_puts "To create a new ${dirpref}irectory, enter the name below and click [cgi_italic "Create New ${Dirpref}irectory"]."
		    cgi_br
		    cgi_br
		    cgi_put "New ${dirpref}irectory name: "
		    cgi_text directory= maxlength=64 size=25%
		    cgi_br
		    cgi_br
		    cgi_submit_button "newdir=Create New ${Dirpref}irectory"
		    cgi_submit_button cancelled=Cancel
		  }
		}

		if {0} {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_br
		    cgi_br
		    cgi_puts "To return to the Folder List page without creating anything click [cgi_italic Cancel]."
		    cgi_br
		    cgi_br
		    cgi_submit_button cancelled=Cancel
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

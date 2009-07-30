#!./tclsh
# $Id: querynewfoldir.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=confirm target=_top {
	cgi_text "page=folders" type=hidden notab
	cgi_text "fid=$fid" type=hidden notab
	cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
	cgi_text "frestore=1" type=hidden notab

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
		    cgi_br
		    cgi_put "Folders are used to contain messages.  Typically, messages are placed in folders when you [cgi_italic Save] them from the Message View"
		    if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
		      cgi_puts " or Message List pages."
		    } else {
		      cgi_puts "page."
		    }

		    cgi_put "To create a new folder"

		    if {[llength $fid] > 1} {
		      cgi_put " within the directory [cgi_bold [join [lrange $fid 1 end] /]]"
		    }
		    if {[llength $collections] > 1} {
		      cgi_put " in the collection [cgi_bold [lindex [lindex $collections [lindex $fid 0]] 1]]"
		    }

		    cgi_put ", enter the name below and click [cgi_italic "Create New Folder"]."
		    cgi_br
		    cgi_br

		    cgi_put "Furthermore, folders can be created within directories.  The directory can either be one that now exists "
		    cgi_put " or one that you wish to create along with the new folder. "
		    cgi_put "Simply specify the directory name before the folder name separating the two with a &quot;[WPCmd PEFolder delimiter [lindex $fid 0]]&quot; character."
		    cgi_br
		    cgi_br
		    cgi_put "New folder name: "
		    cgi_text folder= maxlength=64 size=25%
		    cgi_br
		    cgi_br
		    cgi_submit_button "newfolder=Create New Folder" "style=\"margin-right: 10px\""
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

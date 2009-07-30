# $Id: exporting.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  exporting.tcl
#
#  Purpose:  CGI script to generate html output associated with folder
#	     exporting explanation text
#
#  Input:
set export_vars {
  {fid		"Missing Collection ID"}
  {cid		"Missing Command ID"}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

# Command Menu definition for Message View Screen
set export_menu {
}

set common_menu {
  {
    {}
    {
      {
	# * * * * Cancel * * * *
	cgi_put [cgi_url "Folder List" wp.tcl?page=folders&cid=[WPCmd PEInfo key] target=_top class=navbar]
      }
    }
  }
}

## read vars
foreach item $export_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {[catch {WPCmd PEInfo key} key]} {
  error [list _action "command ID" $key]
}

# massage fid, strip leading "f_"
set fid [string range [lindex $fid 0] 2 end]
set digfid [cgi_unquote_input $fid]
set colid [lindex $digfid 0]
if {[set l [llength $digfid]] > 2} {
  set fpath [eval "file join [lrange $digfid 1 [expr {[llength $digfid] - 1}]]"]
} else {
  set fpath ""
}
set fldr [lindex $digfid end]

# paint the page
cgi_http_head {
  WPStdHttpHdrs text/html
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "Folder Exporting"
    WPStyleSheets
    cgi_http_equiv Refresh "0; url=$_wp(serverpath)/$_wp(appdir)/$_wp(ui1dir)/export.tcl?fid=${fid}&cid=$cid"
  }

  cgi_body bgcolor=$_wp(bordercolor) {

    set mbox [WPCmd PEMailbox mailboxname]

    WPTFTitle "Folder Export"

    cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {

      cgi_table_row {
	cgi_table_data rowspan=2 valign=top class=navbar {
	  cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=2 {
	    cgi_table_row {
	      cgi_table_data class=navbar style=padding-top:6 {
		cgi_puts "Current Folder :"
		cgi_division align=center "style=margin-top:4;margin-bottom:4" {
		  cgi_put [cgi_url [WPCmd PEMailbox mailboxname] fr_main.tcl target=_top class=navbar]

		  switch -exact -- [WPCmd PEMailbox state] {
		    readonly {
		      cgi_br
		      cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Read Only)"]
		    }
		    closed {
		      cgi_br
		      cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Closed)"]
		    }
		    ok -
		    default {}
		  }

		  cgi_br
		}

		cgi_hr "width=75%"
	      }
	    }

	    # next comes the menu down the left side, with suitable
	    cgi_table_row {
	      eval {
		cgi_table_data $_wp(menuargs) class=navbar style=padding-bottom:10 {
		  WPTFCommandMenu export_menu common_menu
		}
	      }
	    }
	  }
	}

	# down the right side of the table is the window's contents
	cgi_table_data width="100%" valign=top class=dialog {

	  cgi_division "style=\"margin-left: 12%; margin-right: 12%\"" {

	      cgi_division align=center "style=\"padding: 18; font-size: bigger \"" {
		cgi_puts "Export Folder"
	      }

	      cgi_puts "WebPine is preparing the folder [cgi_bold $fldr] for download. "
	      cgi_puts "You should see your browser's File Open Dialog appear any momment."

	      cgi_p

	      cgi_puts "The exported file will contain all of the messages in the folder separated "
	      cgi_puts "by a traditional mail message delimiter, and should be recognizable by "
	      cgi_puts "a variety of desktop mail programs."

	      cgi_p

	      cgi_puts "Be sure to pick a good name for the downloaded mail folder."
	      cgi_puts "If you are sure the folder has been exported properly (that is, "
	      cgi_puts "there were no error messages or other such problems, you can "
	      if {[string compare inbox [string tolower $mbox]]} {
		cgi_puts "delete the folder from the collection."
	      } else {
		cgi_puts "delete and expunge the messages from your INBOX."
	      }

	      cgi_p 

	      cgi_puts "WebPine's [cgi_span "style=font-weight: bold; font-style: italic" Import] command, found to the right of each collection and "
	      cgi_puts "directory entry in the folder list, can be used to transfer the exported "
	      cgi_puts "mail folder from your computer back into a folder collection "
	      cgi_puts "suitable for viewing within WebPine "

	      cgi_p

	      cgi_puts "If your browser does not automatically return to the Folder List page after the download is complete, click the button below."

	      cgi_p

	      cgi_division align=center {
		cgi_form $_wp(serverpath)/$_wp(appdir)/$_wp(ui1dir)/wp.tcl method=get {
		  cgi_text "page=folders" type=hidden notab
		  cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab

		  cgi_submit_button "done=Return to Folder List"
		}
	      }
	    }
	  }
	}
      }
    }
  }


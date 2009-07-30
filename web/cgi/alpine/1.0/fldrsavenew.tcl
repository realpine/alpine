# $Id: fldrsavenew.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fldrsavenew.tcl
#
#  Purpose:  CGI script to generate html output associated with message
#	     Save to a new folder
#
#  Input:
set folder_vars {
  {onselect	""	"index"}
  {oncancel	""	"index"}
  {target	""	""}
  {controls	""	1}
  {reload}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

set key [WPCmd PEInfo key]

#
# Command Menu definition for Message View Screen
#
set folder_menu {
}

set common_menu {
  {
    {expr {$controls == 1}}
    {
      {
	# * * * * CANCEL * * * *
	cgi_puts [cgi_url Cancel wp.tcl?page=$oncancel&cid=[WPCmd PEInfo key] class=navbar target=_top]
      }
    }
  }
  {}
  {
    {}
    {
      {
	cgi_puts "Get Help"
      }
    }
  }
}


## read vars
foreach item $folder_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {[catch {WPNewMail $reload} newmail]} {
  error [list _action "new mail" $newmail]
}

# perform any requested actions

# preserve vars that my have been overridden with cgi parms

# paint the page
cgi_http_head {
  WPStdHttpHdrs text/html
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "Folder Create for Save" folders
    if {$controls == 1} {
      WPHtmlHdrReload "$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=fldrsavenew"
    }

    WPStyleSheets
  }

  cgi_body onload=document.savepmt.f_name.focus() bgcolor=$_wp(bordercolor) {

    catch {WPCmd PEInfo set help_context fldrsave}

    # prepare context and navigation information

    set navops ""

    if {$controls == 1} {
      WPTFTitle "Browse Folder" $newmail
    }

    cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
      cgi_table_row {
	if {$controls > 0} {
	  cgi_table_data rowspan=2 valign=top class=navbar {
	    cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=2 {
	      # next comes the menu down the left side, with suitable
	      cgi_table_row {
		eval {
		  cgi_table_data $_wp(menuargs) class=navbar {
		    WPTFCommandMenu folder_menu common_menu
		  }
		}
	      }
	    }
	  }
	}

	# down the right side of the table is the window's contents
	cgi_table_data width="100%" align=center valign=top class=dialog {

	  if {[string length $target] == 0} {
	    set target _top
	  }

	  cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=$target name=savepmt {
	    cgi_text page=[lindex $onselect 0] type=hidden notab
	    for {set i 1} {$i < [llength $onselect]} {incr i} {
	      set a [split [lindex $onselect $i] {=}]
	      cgi_text [lindex $a 0]=[lindex $a 1] type=hidden notab
	    }

	    cgi_text cid=[WPCmd PEInfo key] type=hidden notab

	    # then the table representing the folders
	    cgi_table width=75% border=0 cellspacing=0 cellpadding=2 align=center {

	      cgi_table_row {
		cgi_table_data height=140 align=center valign=middle class=dialog {
		  cgi_p "Enter the name of the folder to save to below, and then click [cgi_italic OK]."
		  cgi_p "You may enter either the name of an existing folder, or the name of a new folder name to have it created. You may also specify a directory path in front of the folder name."
		  cgi_p "Click [cgi_italic Cancel] to return without saving (or creating) anything."
		}
	      }

	      if {[WPCmd PEFolder isincoming 0]} {
		set f_colid 1
	      } else {
		set f_colid 0
	      }

	      cgi_table_row {
		cgi_table_data align=center valign=top height=40 class=dialog {
		  cgi_put "Folder name : "
		  cgi_text f_name= type=text size=20 maxlength=256 style=vertical-align:middle onFocus=this.select()

		  if {[catch {WPCmd PEFolder collections} collections] == 0 && [llength $collections] > 1} {

		    cgi_put "within "

		    cgi_select f_colid style=vertical-align:middle {
		      set j 0
		      foreach i $collections {
			if {$j == $f_colid} {
			  set selected selected
			} else {
			  set selected {}
			}
			if {[string length [set f [lindex $i 1]]] > 12} {
			  set f "[string range $f 0 10]..."
			}

			cgi_option $f value=$j $selected
			incr j;
		      }
		    }
		  } else {
		    cgi_text f_colid=0 type=hidden notab
		  }
		}
	      }

	      cgi_table_row {
		cgi_table_data align=center valign=middle height=60 class=dialog {
		  cgi_submit_button "ok=  OK  "
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


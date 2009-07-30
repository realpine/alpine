#!./tclsh
# $Id: promptsave.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  promptsave.tcl
#
#  Purpose:  CGI script to generate html form used to gather folder
#            name and collection for aggregate save

#  Input:
#     conftext : 
#     params : array of key/value pairs to submit with form
#     oncancel : url to reference should user cancel dialog
set psave_vars {
  {uid	""	0}
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

WPEval $psave_vars {
  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Aggregate Save"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      catch {WPCmd PEInfo set help_context promptsave}
      catch {WPCmd PEInfo set wp_index_script fr_promptsave.tcl}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=auth target=body {
	cgi_text page=body type=hidden notab

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    cgi_table_data align=center valign=top class=dialog {
	      cgi_table width="80%" {
		cgi_table_row {
		  cgi_table_data colspan=2 {
		    cgi_center {
		      cgi_puts "[cgi_nl][cgi_nl]This page provides a way to save messages to a folder"
		    }
		  }

		  cgi_table_row class=dialog {
		    cgi_table_data valign=top align=center nowrap class=dialog colspan=2 {
		      cgi_puts [cgi_font face=tahoma,verdana,geneva "Save for messages "]

		      cgi_put "Save "

		      if {!$uid} {
			set n [WPCmd PEMailbox selected]
			cgi_put "all [cgi_bold [WPcomma $n]] marked message[WPplural $n] "
			if {[catch {WPCmd PEMessage $uid savedefault} savedefault]} {
			  set savedefault [list 1 saved-messages]
			}
		      } else {
			set savedefault [list 1 saved-messages]
		      }

		      cgi_put "to "
		      cgi_br

		      cgi_text "savename=[lindex $savedefault 1]" type=text size=14 maxlength=256 class=aggop style=vertical-align:middle onFocus=this.select()
		      if {[catch {WPCmd PEFolder collections} collections] == 0 && [llength $collections] > 1} {
			cgi_put "[cgi_nbspace]in "
			cgi_select savecolid class=aggop style=vertical-align:middle {
			  set defcol [lindex $savedefault 0]
			  set j 0
			  foreach i $collections {
			    if {$j == $defcol} {
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
			cgi_text "savecolid=0" type=hidden notab
		      }

		      cgi_br
		      cgi_puts "[cgi_nl]Click [cgi_italic Save] to save the message the folder, or [cgi_italic Cancel] to abort the save."
		    }
		  }

		  cgi_table_row {
		    cgi_table_data class=dialog align=center colspan=2 {
		      cgi_br
		      cgi_submit_button save=Save
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

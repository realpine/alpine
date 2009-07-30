#!./tclsh
# $Id: querynick.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  querynick.tcl
#
#  Purpose:  CGI script to generate html form used to deal
#            with existing/taken nickname collision
#

#  Input:
set nick_vars {
  {book	"Missing address book"}
  {nick       {} ""}
  {add        {} 0}
  {fn         {} ""}
  {addrs      {} ""}
  {fcc        {} ""}
  {comment    {} ""}
  {take       {} 0}
  {newnick    {} ""}
  {ai         {} -1}
}

#  Output:
#

# Command Menu
set nick_menu {
}

set common_menu {
  {
    {}
    {
      {
	cgi_puts "Get Help"
      }
    }
  }
}


#  Output:
#     Query prompt to deal with existing/taken nickname collision
#
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

WPEval $nick_vars {

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Take to New Entry or List"
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      #catch {WPCmd PEInfo set help_context samenick}

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=samenick target=_top {

	cgi_table border=0 cellspacing=0 cellpadding=2 height="100%" {
	  cgi_table_row {
	    cgi_table_data valign=top class=navbar {
	      cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=2 {
		# next comes the menu down the left side, with suitable
		cgi_table_row {
		  eval {
		    cgi_table_data $_wp(menuargs) class=navbar {
		      WPTFCommandMenu nick_menu common_menu
		    }
		  }
		}
	      }
	    }

	    cgi_table_data valign=top class=navbar {

	      cgi_text "page=addrsave" type=hidden notab
	      cgi_text "oncancel=main" type=hidden notab
	      cgi_text "take=1" type=hidden notab
	      cgi_text "ai=${ai}" type=hidden notab
	      cgi_text "book=${book}" type=hidden notab
	      cgi_text "nick=${nick}" type=hidden notab
	      cgi_text "fn=${fn}" type=hidden notab
	      cgi_text "addrs=${addrs}" type=hidden notab
	      cgi_text "fcc=${fcc}" type=hidden notab
	      cgi_text "comment=${comment}" type=hidden notab
	      cgi_text "newnick=${newnick}" type=hidden notab
	      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab

	      cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
		cgi_table_row {
		  cgi_table_data valign=top align=center class=dialog {
		    cgi_table border=0 cellspacing=0 cellpadding=15 width="75%" {
		      cgi_table_row {
			cgi_table_data align=center colspan=2 "xstyle=padding-top:20;padding-bottom:20" {
			  cgi_puts "An address book entry with the nickname \"[cgi_bold $newnick]\" already exists.  At this point you may click either:"
			}
		      }

		      cgi_table_row {
			cgi_table_data align=right {
			  cgi_submit_button "replace=Replace Entry"
			}
			cgi_table_data "xstyle=padding:15" {
			  cgi_puts "Replace the \"[cgi_bold $newnick]\" address book entry with your [cgi_italic Take] selection."
			}
		      }

		      cgi_table_row {
			cgi_table_data align=right {
			  cgi_submit_button "replace=Add to Entry"
			}
			cgi_table_data "xstyle=padding:15" {
			  if {[string first "," $addrs] >= 0} {
			    set plur "es"
			  } else {
			    set plur ""
			  }

			  cgi_puts "Add the address${plur} from your [cgi_italic Take] selection to the existing entry's addresses to create a list."
			}
		      }

		      cgi_table_row {
			cgi_table_data align=right {
			  cgi_submit_button "replace=Edit"
			}
			cgi_table_data "xstyle=padding:15" {
			  cgi_puts "Go back to editing your [cgi_italic Take] selection."
			}
		      }

		      cgi_table_row {
			cgi_table_data align=right {
			  cgi_submit_button "cancel=Cancel"
			}
			cgi_table_data "xstyle=padding:15" {
			  cgi_puts "Or, Cancel your [cgi_italic Take] selection altogether."
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
  }
}

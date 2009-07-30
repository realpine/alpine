#!./tclsh
# $Id: takeaddr.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  takeaddr.tcl
#
#  Purpose:  CGI script to take addresses to address book

#  Input:
set takeaddr_vars {
  {uid	"Missing UID"}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

# Command Menu definition for Message View Screen
set take_menu {
}

set common_menu {
  {
    {}
    {
      {
	cgi_put [cgi_url "Get Help" wp.tcl?page=help&topic=take&index=none&oncancel=view%2526op%253DTake class=navbar target=_top]
      }
    }
  }
}

WPEval $takeaddr_vars {

  if {[catch {WPCmd PEInfo noop} result]} {
    error [list _action "No Op" $result "Please close this window."]
  }

  if {$uid > 0 && [catch {WPCmd PEMessage $uid takeaddr} tainfo]} {
    error [list _action "takeaddr $uid" $tainfo "Click Browsers Back Button."]
  }
  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Take Address"
      WPStyleSheets
    }

    cgi_body bgcolor=$_wp(bordercolor) {
      cgi_table border=0 cellspacing=0 cellpadding=0 height="100%" {
	cgi_table_row {
	  cgi_table_data valign=top class=navbar width=112 {
	    cgi_table bgcolor=$_wp(menucolor) border=0 "style=\"padding-left: 2\"" {
	      # next comes the menu down the left side, with suitable
	      cgi_table_row {
		eval {
		  cgi_table_data $_wp(menuargs) class=navbar {
		    WPTFCommandMenu take_menu common_menu
		  }
		}
	      }
	    }
	  }

	  cgi_table_data valign=top class=navbar {
	    cgi_table border=0 cellspacing=0 cellpadding=2 align=center valign=top height="100%" {
	      cgi_table_row {
		cgi_table_data valign=top class=dialog {
		  cgi_table align=center border=0 width=75% height=80 class=dialog {
		    cgi_table_row {
		      cgi_table_data class=dialog align=center valign=middle {
			set txt "Select the address that you would like to take to your address book"
			if {[llength $tainfo] > 1} {
			  set txt "$txt, or select multiple addresses to create a list."
			} else {
			  set txt "$txt."
			}
			append txt "When finished, click [cgi_italic "Take Address"] to create an entry"
			append txt " or [cgi_italic "Cancel"] to return, creating nothing."
			#cgi_puts [cgi_bold $txt ]
			cgi_puts $txt
		      }
		    }
		  }
		}
	      }

	      cgi_table_row {
		cgi_table_data valign=top height=99% class=dialog {

		  set books [WPCmd PEAddress books]

		  cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post target=_top name=takeaddr {
		    cgi_text page=view type=hidden notab
		    cgi_text uid=$uid type=hidden notab
		    cgi_text cid=[WPCmd PEInfo key] type=hidden notab
		    if {[llength $books] <= 1} {
		      cgi_text "book=0" type=hidden notab
		    }

		    cgi_table border=0 cellspacing=0 cellpadding=2 align=center valign=top width="70%" height="100%" {
		      if {[llength $books] > 1} {
			cgi_table_row {
			  cgi_table_data colspan=2 valign=top align=center class=dialog {
			    cgi_table border=0 cellspacing=0 cellpadding=0 "style=padding-top:20;padding-bottom:20" {
			      cgi_table_row {
				cgi_table_data align=right {
				  cgi_puts [cgi_bold "Take to address book"]
				}
				cgi_table_data align=right valign=top {
				  cgi_puts [cgi_bold "[cgi_nbspace]:[cgi_nbspace]"]
				}
				cgi_table_data align=left {
				  cgi_select book {
				    foreach book $books {
				      cgi_option [lindex $book 1] value=[lindex $book 0]
				    }
				  }
				}
			      }
			    }
			  }
			}
		      }

		      set linenum 1
		      foreach taline $tainfo {
			set printstr [lindex $taline 0]
			set addr [lindex $taline 1]
			set sugs [lindex $taline 2]
			if {[llength $addr] == 0 && [llength $sugs] == 0} {
			  cgi_table_row {
			    cgi_table_data align=center class=dialog colspan=2 "style=xpadding-left:8%;xpadding-right:8%;padding-top:20;padding-bottom:20" {
			      cgi_put $printstr
			    }
			  }

			  continue
			}
			
			if {[incr linenum] % 2} {
			  set bgcolor #EEEEEE
			} else {
			  set bgcolor #FFFFFF
			}

			cgi_table_row bgcolor=$bgcolor {
			  cgi_table_data align=right {
			    cgi_checkbox "taList=tl${linenum}" style=background-color:$bgcolor
			    if {[llength $addr] > 0} {
			      if {[set tmp [lindex $addr 0]] != {}} {
				cgi_text "tl${linenum}p=$tmp" type=hidden notab
			      }
			      if {[set tmp [lindex $addr 1]] != {}} {
				cgi_text "tl${linenum}m=$tmp" type=hidden notab
			      }
			      if {[set tmp [lindex $addr 2]] != {}} {
				cgi_text "tl${linenum}h=$tmp" type=hidden notab
			      }
			    }
			    if {[llength $sugs] > 0} {
			      if {[set tmp [lindex $sugs 0]] != {}} {
				cgi_text "tl${linenum}n=$tmp" type=hidden notab
			      }
			      if {[set tmp [lindex $sugs 1]] != {}} {
				cgi_text "tl${linenum}fn=$tmp" type=hidden notab
			      }
			      if {[set tmp [lindex $sugs 2]] != {}} {
				cgi_text "tl${linenum}fcc=$tmp" type=hidden notab
			      }
			      if {[set tmp [lindex $sugs 3]] != {}} {
				cgi_text "tl${linenum}c=$tmp" type=hidden notab
			      }
			    }
			  }
			  cgi_table_data align=left {
			    regsub -all "<" $printstr "\\&lt;" printstr
			    regsub -all ">" $printstr "\\&gt;" printstr
			    cgi_puts $printstr
			  }
			}
		      }

		      cgi_table_row {
			cgi_table_data height="99%" colspan=2 valign=top align=center class=dialog "style=padding-top:16" {
			  cgi_submit_button "op=Take Address"
			  cgi_submit_button takecancel=Cancel
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

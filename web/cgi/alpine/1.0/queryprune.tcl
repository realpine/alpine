#!./tclsh
# $Id: queryprune.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# queryprune.tcl
#
# After we've already determined that it's the 
# beginning of the month (logon.tcl), we check
# what folders need pruning and offer them to
# the user.  Currently doesn't do automatic
# reload.

# Input:
set prunetime_vars {
  {cid	"Missing Command ID"}
  {start "Missing Start Page"}
  {nojs	""			0}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set prune_menu {
  {
    {}
    {
      {
	# * * * * DONE * * * *
	cgi_submit_button prune=Continue
      }
    }
  }
}


WPEval $prunetime_vars {
  catch {WPCmd PEInfo prunetime} prunefldrs
  set allclean 1
  set delstuff 0
  set askstuff 0
  foreach prunefldr $prunefldrs {
    if {[llength [lindex $prunefldr 1]] > 0 || [llength [lindex $prunefldr 2]] > 0} {
      set allclean 0
    }
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Monthly Folder Pruning"
      WPStdScripts
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      cgi_form [file join $_wp(appdir) $_wp(ui1dir) wp] method=post name=pruneit target=_top {

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  
	  cgi_table_row {
	    # next comes the menu down the left side
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu prune_menu {}
	      }
	    }

	    cgi_table_data valign=top align=center class=dialog "style=\"padding: 20\"" {

	      if {$allclean == 1} {
		catch {WPCmd PEInfo statmsg "Pruning Failed: $prunefldrs"}
		cgi_puts "No folders appear to need cleaning up this month."
		cgi_br
		cgi_puts "Please click [cgi_url "here" $start target=_top] to continue your session."
	      } else {
		cgi_puts "At the beginning of every month, you are asked if you would like to clean up your sent-mail folder(s).  Please answer the following questions and click [cgi_italic Continue]."
		cgi_text "sessid=$_wp(sessid)" type=hidden notab
		cgi_text "op=pruneit" type=hidden notab
		cgi_text "cid=${cid}" type=hidden notab
		cgi_text "page=prune" type=hidden notab
		set cnt 0
		foreach prunefldr $prunefldrs {
		  set type [lindex $prunefldr 0]
		  set mv [lindex $prunefldr 1]
		  set dellist [lindex $prunefldr 2]

		  cgi_table border=0 cellpadding=8 cellspacing=0 "style=\"padding-top: 8\"" {
		    if {[llength $mv] > 1} {
		      cgi_table_row {
			cgi_table_data {
			  cgi_puts [cgi_bold "Move current &quot;[lindex $mv 0]&quot; to &quot;[lindex $mv 1]&quot;?"]
			}
		      }
		      cgi_table_row {
			cgi_table_data {
			  cgi_table "style=\"padding-left: 20\"" {
			    cgi_table_data {
			      cgi_radio_button "mv${cnt}=mv [lindex $mv 0] [lindex $mv 1]" checked class=body
			    }
			    cgi_table_data {
			      cgi_puts "Yes"
			    }
			    cgi_table_data {
			      cgi_radio_button "mv${cnt}=" class=body
			    }
			    cgi_table_data {
			      cgi_puts "No"
			    }
			  }
			}
		      }
		      incr cnt
		    }
		    if {[llength $dellist] > 0} {
		      cgi_table_row {
			cgi_table_data {
			  set plurtxt ""
			  set typetxt ""
			  if {[llength $dellist] > 1} {
			    set plurtxt "s"
			  }
			  if {[string compare $type ""] != 0} {
			    set typetxt "[string toupper $type] "
			  }
			  cgi_puts "[cgi_bold "To save disk space, delete the following ${typetxt}mail folder${plurtxt}:"] (Check to delete)"
			}
		      }

		      cgi_table_row {
			cgi_table_data {
			  cgi_table "style=\"padding-left: 20\"" {
			    foreach del $dellist {
			      cgi_table_row {
				cgi_table_data {
				  cgi_checkbox "delList=$del" "style=\"background-color: #FFFFFF; padding-right: 8\""
				  cgi_puts "$del "
				}
			      }
			    }
			  }
			}
		      }
		    }
		  }
		}
		cgi_text "mvcnt=${cnt}" type=hidden notab
	      }
	    }
	  }
	}
      }
    }
  }
}

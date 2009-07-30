#!./tclsh
# $Id: queryquit.tcl 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $
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

#  queryquit.tcl
#
#  Purpose:  CGI script to generate html form used to confirm quitting
#            webpine while offering to expunge deleted

#  Input:
#     conftext : 
#     params : array of key/value pairs to submit with form
#     oncancel : url to reference should user cancel confirmation
set quit_vars {
  {cid	"Command ID"}
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
	cgi_image_button quit=[WPimg but_create] border=0 alt="Create"
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

WPEval $quit_vars {

  if {$cid != [WPCmd PEInfo key]} {
    error "Invalid Command ID"
  }

  catch {WPCmd PESession expungecheck quit} prompts

  set qhid ""
  set delsexist 0
  set askinbox 1
  set askcurrent 1
  set ewc [WPCmd PEInfo feature expunge-without-confirm]
  set ewce [WPCmd PEInfo feature expunge-without-confirm-everywhere]

  foreach prompt $prompts {
    if {[lindex $prompt 1] > 0} {
      set delsexist 1
    }
    if {[lindex $prompt 1] > 0 && ($ewc || $ewce) && [lindex $prompt 2] == 1} {
      set askinbox 0
      lappend qhid [cgi_buffer {cgi_text expinbox=1 type=hidden notab}]
    } elseif {[lindex $prompt 1] > 0 && [lindex $prompt 2] == 0 && ($ewce || ($ewc && [lindex $prompt 3] == 1))} {
      set askcurrent 0
      lappend qhid [cgi_buffer {cgi_text expcurrent=1 type=hidden notab}]
    }
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Quitting Web Alpine"
      WPStdScripts
      WPStyleSheets
      cgi_put  "<style type='text/css'>"
      cgi_put  ".expungebox { background-color:AntiqueWhite }"
      cgi_put  ".clickit { cursor: pointer }"
      cgi_puts "</style>"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      #catch {WPCmd PEInfo set help_context quit}
      cgi_table width=100% height=100% cellspacing=0 cellpadding=0 {
	cgi_table_row {
	  cgi_table_data width=112 bgcolor=$_wp(bordercolor) {
	    cgi_put [cgi_img [WPimg dot2]]
	  }

	  cgi_table_data align=center valign=top bgcolor="$_wp(dialogcolor)" {

	    cgi_form $_wp(appdir)/$_wp(ui1dir)/do_quit method=get id=quitting target=_top {
	      cgi_text cid=$cid type=hidden notab
	      cgi_text sessid=$sessid type=hidden notab
	      foreach q $qhid {
		cgi_puts $q
	      }

	      cgi_table border=0 cellspacing=0 cellpadding=10 width="70%" class=dialog "style=padding-top:12%" {
		cgi_table_row {
		  cgi_table_data valign=top {
		    cgi_puts [cgi_font size=+1 "Really Quit WebPine?"]
		  }
		}
		cgi_table_row {
		  cgi_table_data valign=top {
		    cgi_table cellpadding=10 border=0 {
		      if {$delsexist} {
			cgi_table_row {
			  cgi_table_data valign=middle class=expungebox "style=\"border: 1px solid red\"" {
			    if {[llength $prompts] > 1} {
			      set ftext "[lindex [lindex $prompts 0] 0] and [lindex [lindex $prompts 1] 0]"
			    } else {
			      set ftext "[lindex [lindex $prompts 0] 0]"
			    }

			    cgi_put "This is a good opportunity to permanently remove from ${ftext} all of the messages you have marked for deletion."
			    cgi_br
			    cgi_br
			    cgi_table border=0 cellpadding=4 {
			      set expiexists 0
			      set expcexists 0
			      set inbhit 0
			      set curhit 0
			      foreach prompt $prompts {
				set numdels [lindex $prompt 1]
				set fname [lindex $prompt 0]
				set inboxflag [lindex $prompt 2]
				set incflag [lindex $prompt 3]
				if {$inboxflag} {
				  incr inbhit
				} else {
				  incr curhit
				}
				if {$numdels && (($askinbox && $inboxflag) || ($askcurrent && $inboxflag == 0))} {
				  cgi_table_row {
				    set cbn "cb[expr {$inbhit + $curhit}]"
				    cgi_table_data align=right valign=top {
				      if {$inboxflag} {
					cgi_checkbox "expinbox" class=expungebox id=$cbn checked
					incr expiexists
				      } else {
					cgi_checkbox "expcurrent" class=expungebox id=$cbn checked
					incr expcexists
				      }
				    }
				    cgi_table_data align=left {
				      set t "Expunge ${numdels} deleted message[expr {($numdels > 1) ? "s" : ""}] from ${fname}."
				      cgi_put [cgi_span class=clickit onclick=\"flipCheck('$cbn')\" $t]
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

		cgi_table_row {
		  cgi_table_data align=center valign=middle {
		    cgi_br
		    cgi_submit_button "quit=Yes, Quit Now"
		    cgi_put [cgi_img [WPimg dot2] width=10]
		    cgi_submit_button "cancel=No, Return to [WPCmd PEMailbox mailboxname]"
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

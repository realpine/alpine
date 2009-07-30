#!./tclsh
# $Id: comview.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# comview.tcl
#
# Purpose:  CGI script to produce the common view commands frame

# Input:
set comview_vars {
  {f_colid	{}	[WPCmd PEFolder isincoming 0]}
  {f_name	{}	"saved-messages"}
}

# inherit global config
source ./alpine.tcl
source ./cmdfunc.tcl

WPEval $comview_vars {
  cgi_http_head {
    WPStdHttpHdrs {} 60
  }

  cgi_html {
    cgi_head {
      if {[info exists _wp(exitonclose)]} {
	WPExitOnClose top.spec.body
      }

      WPStyleSheets
      cgi_put  "<style type='text/css'>"
      cgi_put  ".viewop { font-family: arial, sans-serif; font-size: 7pt }"
      cgi_puts "</style>"
      if {$_wp(keybindings)} {
	set kequiv {
	  {{?} {top.location = 'wp.tcl?page=help'}}
	  {{l} {top.location = 'wp.tcl?page=folders'}}
	  {{a} {top.location = 'wp.tcl?page=addrbook'}}
	  {{n} {top.spec.body.location = 'wp.tcl?page=view&bod_next=1'}}
	  {{p} {top.spec.body.location = 'wp.tcl?page=view&bod_prev=1'}}
	  {{i} {top.spec.location = 'fr_index.tcl'}}
	  {{s} {document.saveform.f_name.focus()}}
	  {{d} {document.delform.op[0].click()}}
	  {{u} {document.delform.op[1].click()}}
	  {{r} {document.replform.op.click()}}
	  {{f} {document.forwform.op.click()}}
	}
	
	lappend kequiv [list {c} "top.location = 'wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key]'"]

	if {[WPCmd PEInfo feature enable-full-header-cmd]} {
	  lappend kequiv [list {h} "top.spec.body.location = 'wp.tcl?page=view&fullhdr=flip'"]
	}

	set onload "onLoad=[WPTFKeyEquiv $kequiv document.saveform.f_name top.spec.body]"
      }
    }

    cgi_body bgcolor=$_wp(bordercolor) background=[file join $_wp(imagepath) logo $_wp(logodir) back.gif] "style=\"background-repeat: repeat-x\"" $onload {
      cgi_table class=ops cellpadding=0 cellspacing=0 border=0 width="100%" height=24 {
	cgi_table_row {
	  cgi_table_data valign=middle align=left nowrap class=viewop {
	    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=_top name=replform {
	      cgi_text "page=view" type=hidden notab
	      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
	      cgi_text "oncancel=main.tcl" type=hidden notab
	      cgi_text "postpost=fr_main.tcl" type=hidden notab

	      cgi_table border=0 class=ops cellpadding=0 cellspacing=0 class=viewop {
		cgi_table_row {
		  cgi_table_data class=viewop rowspan=2 {
		    # * * * * REPLY * * * *
		    cgi_submit_button op=Reply class="viewop" "style=\"vertical-align: middle; margin-left: 4\""
		  }
		  cgi_table_data class=viewop {
		    cgi_checkbox "repall=1" style=vertical-align:middle
		    cgi_put "To All[cgi_nbspace]"
		  }
		}
		cgi_table_row {
		  cgi_table_data class=viewop rowspan=2 {
		    cgi_checkbox "reptext=1" checked style=vertical-align:middle
		    cgi_put "Include text"
		  }
		}
	      }
	    }
	  }

	  cgi_table_data valign=middle align=center {
	    cgi_put [cgi_img [WPimg blackdot] width=1 height=26]
	  }

	  cgi_table_data valign=middle align=center nowrap {
	    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=_top name=forwform {
	      cgi_text "page=view" type=hidden notab
	      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
	      cgi_text "oncancel=main.tcl" type=hidden notab
	      cgi_text "postpost=fr_main.tcl" type=hidden notab

	      # * * * * FORWARD * * * *
	      cgi_submit_button op=Forward class="viewop"
	    }
	  }

	  cgi_table_data valign=middle align=center {
	    cgi_put [cgi_img [WPimg blackdot] width=1 height=26]
	  }

	  cgi_table_data valign=middle align=center nowrap class=viewop {
	    cgi_table class=ops cellpadding=0 cellspacing=0 border=0 class=viewop {
	      cgi_table_row {
		cgi_table_data class=viewop {
		  cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=spec name=saveform  {
		    cgi_text "page=fr_view" type=hidden notab
		    cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
		    cgi_text "sid=[clock seconds]" type=hidden notab

		    # * * * * Save * * * *
		    cgi_submit_button "save=Save" class="viewop"
		    cgi_put "[cgi_nbspace]to "

		    cgi_text f_colid=$f_colid type=hidden notab
		    cgi_text op=save type=hidden notab

		    cgi_select f_name class=viewop style=vertical-align:middle "onchange=document.saveform.save.click(); return false;" {
		      foreach {oname oval} [WPTFGetSaveCache] {
			cgi_option $oname value=$oval
		      }
		    }
		  }
		}
	      }
	    }
	  }

	  cgi_table_data valign=middle align=center {
	    cgi_put [cgi_img [WPimg blackdot] width=1 height=26]
	  }

	  cgi_table_data valign=middle align=center nowrap {
	    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=_top name=take {
	      cgi_text "page=view" type=hidden notab
	      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
	      cgi_submit_button op=Take class="viewop"
	    }
	  }

	  cgi_table_data valign=middle align=center {
	    cgi_put [cgi_img [WPimg blackdot] width=1 height=26]
	  }

	  cgi_table_data valign=middle align=right nowrap {
	    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=body name=delform {
	      cgi_text "page=view" type=hidden notab
	      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab

	      # * * * * UNDELETE * * * *
	      cgi_submit_button op=Delete class="viewop"

	      # * * * * UNDELETE * * * *
	      cgi_submit_button op=Undelete class="viewop" "style=\"margin-right: 4\""

	      # * * * * ANTISPAM * * * *
	      if {([info exists _wp(spamaddr)] && [string length $_wp(spamaddr)])
		  || ([info exists _wp(spamfolder)] && [string length $_wp(spamfolder)])} {
		cgi_submit_button "op=Report Spam" class="viewop" "style=\"margin-right: 4; color: white; background-color: black\""
	      }
	    }
	  }
	}
      }
    }
  }
}

# $Id: do_view.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  do_view.tcl
#
#  Purpose:  CGI script to serve as the frame-work for including
#	     supplied script snippets that generate the various 
#	     javascript-free webpine pages
#
#  Input:
set view_vars {
  {uid		{}	0}
  {op		{}	""}
  {f_colid	{}	""}
  {f_name	{}	""}
  {savecancel	{}	""}
  {sid		{}	""}
  {auths	{}	0}
  {user		{}	""}
  {pass		{}	""}
  {create	{}	0}
}

## read vars
foreach item $view_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

WPCmd PEInfo set wp_spec_script fr_view.tcl

proc statmsg {msg} {
  catch {WPCmd PEInfo statmsg $msg}
}

if {$uid} {
  # commit to servlet, view will retrieve it
  WPCmd set uid $uid
}

set uidparm ""

# If there's an "op" (meaning we got here from a comview.tcl
# reference) trust "uid" is set within alpined's interpreter.
if {[string length $op]} {
# Set "op" only after we've concluded the folder's ready
# for view.tcl to do the actual copy
  if {[string compare save [string tolower $op]]} {
    append uidparm "&op=$op"
  }
} else {
  append uidparm "&uid=$uid"
}

if {[string length $savecancel]} {
  append uidparm "&savecancel=$savecancel"
}


# handle doing the actual save mechanics here rather than
# view.tcl since the result will have to be reflected
# in comview.tcl's Save dropdown.
if {[string compare save [string tolower $op]] == 0} {
  if {[string length $savecancel] == 0} {
    if {[string length [set folder [string trim $f_name]]]} {
      switch -exact -- $folder {
	__folder__prompt__ {
	  set uid 0
	  _cgi_set_uservar onselect {fr_view op=save}
	  _cgi_set_uservar oncancel fr_view
	  _cgi_set_uservar target spec
	  _cgi_set_uservar controls 0
	  source [WPTFScript savecreate]
	  set nopage 1
	}
	__folder__list__ {
	  set uid 0
	  _cgi_set_uservar onselect {fr_view op=save}
	  _cgi_set_uservar oncancel fr_view
	  _cgi_set_uservar target spec
	  _cgi_set_uservar controls 0
	  source [WPTFScript savebrowse]
	  set nopage 1
	}
	default {
	  if {$uid > 0} {
	    if {$auths} {
	      catch {WPCmd PESession nocred $f_colid $folder}
	      if {[catch {WPCmd PESession creds $f_colid $folder $user $pass} result]} {
		statmsg "Cannot set credentials ($f_colid) $folder: result"
	      }
	    }

	    if {[catch {WPCmd PEFolder exists $f_colid $folder} reason]} {
	      if {[string compare BADPASSWD [string range $reason 0 8]] == 0} {
		set oncancel "view.tcl&uid=$uid&savecancel=1"
		set conftext "Create Folder '$folder'?"
		lappend params [list page fr_view]
		lappend params [list uid $uid]
		lappend params [list op save]
		lappend params [list f_name $folder]
		lappend params [list f_colid $f_colid]
		source [WPTFScript auth]
		set nopage 1
	      } else {
		statmsg "Existance test failed: $reason"
	      }
	    } elseif {$reason == 0} {
	      if {$create == 1 || [string compare create [string tolower $create]] == 0} {
		if {[catch {WPCmd PEFolder create $f_colid $folder} reason]} {
		  statmsg "Create failed: $reason"
		} else {
		  set dosave 1
		}
	      } else {
		#set oncancel "view&uid=$uid&savecancel=1"
		set qstate [list $folder]
		set params [list [list page fr_view]]
		lappend params [list uid $uid]
		lappend params [list sid [clock seconds]]
		lappend params [list op save]
		lappend params [list f_name $folder]
		lappend params [list f_colid $f_colid]
		lappend qstate $params

		if {[catch {WPCmd PEInfo set querycreate_state $qstate}] == 0} {
		  source [WPTFScript querycreate]
		  set nopage 1
		} else {
		  statmsg "Error saving creation state"
		}
	      }
	    } else {
	      set dosave 1
	    }

	    if {[info exists dosave]} {
	      append uidparm "&op=save"
	    }
	  } else {
	    statmsg "Cannot Save unknown message ID"
	  }
	}
      }
    } else {
      statmsg "Cannot Save to emtpy folder name"
    }
  }
}


if {![info exists nopage]} {
  cgi_http_head {
    WPStdHttpHdrs {} 60
  }

  cgi_html {
    cgi_head {
    }

    cgi_frameset "rows=38,*" border=0 frameborder=0 framespacing=0 {

      if {[string length $f_colid] && [string length $f_name]} {
	set parms "&f_colid=${f_colid}&f_name=[WPPercentQuote ${f_name}]"
	if {[string length $sid]} {
	  append parms "&sid=$sid"
	}
      } else {
	set parms ""
      }

      cgi_frame cmds=comview.tcl?c=[WPCmd PEInfo key]${parms} scrolling=no title="Message Commands"
      cgi_frame body=wp.tcl?page=view${uidparm}${parms} title="Message Text"
    }
  }
}

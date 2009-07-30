# $Id: dosend.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  post.tcl
#
#  Purpose:  CGI script to perform message posting via compose.tcl
#	     generated form
#
#  Input: 
set post_vars {
  {cid		"Missing Command ID"}
  {postpost	""			"main.tcl"}
  {user		""			""}
  {pass		""			""}
  {server	""			""}
}

#  Output: 
#

## read vars
foreach item $post_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      error [list _action "Impart Variable" $result]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {$cid != [WPCmd PEInfo key]} {
  error [list _action Postpone "Invalid Operation ID" "Click Back button to try again."]
}

if {[string length $user] && [string length $pass] && [string length $server]} {
  set cclientname "\{$server\}"
  catch {WPCmd PESession nocred 0 $cclientname}
  if {[catch {WPCmd PESession creds 0 $cclientname $user $pass} result]} {
    WPCmd PEInfo statmsg "Cannot set credentials for $server"
  }
}

if {[catch {WPCmd PEInfo set suspended_composition} msgdata] == 0} {
  if {[catch {WPCmd PECompose post $msgdata} errstr]} {
    if {[string compare BADPASSWD [string range $errstr 0 8]] == 0
	&& [catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
	set oncancel "page=compose&restore=1&cid=$cid"
	set conftext "Send Messsage?"
	set reason "The server used to send this message requires authentication.[cgi_nl][cgi_nl]Enter Username and Password to connect to $server"
	_cgi_set_uservar params [WPPercentQuote [list [list server $server] [list page dosend] [list postpost $postpost]]]
	set src auth
    } else {
      # regurgitate the compose window
      _cgi_set_uservar style ""
      #set style ""
      set title "Send Error: [cgi_font class=notice "$errstr"]"
      if {[string length $errstr]} {
	set notice "Send FAILED: $errstr"
      } else {
	set notice "Send FAILED: [WPCmd PEInfo statmsg]"
      }
      WPCmd PEInfo statmsg "$notice"

      if {[info exists attachments]} {
	set a [split $attachments ","]
	unset attachments
	foreach id $a {
	  # id file size type/subtype
	  if {[catch {WPCmd PECompose attachinfo $id} result]} {
	    WPCmd PEInfo statmsg $result
	  } else {
	    lappend attachments [list $id [lindex $result 0] [lindex $result 1] [lindex $result 2]]
	  }
	}
      }

      set src compose
    }
  } else {
    catch {WPCmd PEInfo unset suspended_composition}
    WPCmd PEInfo statmsg "Message Sent!"
    set src $postpost
  }
} else {
  WPCmd PEInfo statmsg "Internal Error: $msgdata"
  set src $postpost
}

source [WPTFScript $src]

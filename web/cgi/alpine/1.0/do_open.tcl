# $Id: do_open.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

if {$cid != [WPCmd PEInfo key]} {
  error [list _action open "Invalid Operation ID" "Click Back button to try again."]
}

if {[catch {WPLoadCGIVar cancel}] == 0 && [string compare Cancel $cancel] == 0} {
  catch {WPCmd PEInfo statmsg "Authentication Cancelled"}
  cgi_http_head {
    cgi_redirect [cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=${oncancel}.tcl
  }
} else {

  if {[catch {WPLoadCGIVar path}]} {
    WPLoadCGIVar colid
    WPLoadCGIVar folder
    set path [list $colid $folder]
  } else {
    set colid [lindex $path 0]
    set folder [lrange $path 1 end]
  }

  if {[info exists src] == 0 && [catch {eval WPCmd PEMailbox open $path} reason]} {
    if {[string first ": no such folder" $reason] > 0} {
      catch {WPCmd PEInfo statmsg "Folder [lrange $path 1 end] doesn't exist"}
      set src $oncancel
    } elseif {[string compare BADPASSWD [string range $reason 0 8]] == 0 || [string compare $reason "Login Error"] == 0} {
      # control error messages
      set statmsgs [WPCmd PEInfo statmsgs]
      WPCmd PEMailbox newmailreset
      if {[catch {WPCmd PESession creds $colid $folder} creds] == 0 && $creds != 0} {
	catch {
	  WPCmd PEInfo statmsg "Invalid Username or Password"
	  WPCmd PESession nocred $colid $folder
	}
      }

      if {[catch {WPCmd PEFolder clextended} coln]} {
	WPCmd set reason "Can't get Collection Info: $coln"
      } else {
	set coln [lindex $coln $colid]
	if {[regexp {^([a-zA-Z\.]+).*\/user=([^ /]*)} [lindex $coln 4] dummy srvname authuser]} {
	  WPCmd set reason "Opening folder [cgi_bold $folder] first requires that you log in o the server [cgi_bold "$srvname"]."
	  WPCmd set authuser $authuser
	} elseif {[WPCmd PEFolder isincoming $colid]} {
	  WPCmd set reason "Incoming folder [cgi_bold $folder] requires you log into the the server."
	} else {
	  WPCmd set reason "Opening [cgi_bold $folder] in [cgi_bold [lindex $coln 1]] requires you log into the the server."
	}
      }

      WPCmd set cid [WPCmd PEInfo key]
      WPCmd set authcol $colid
      WPCmd set authfolder $folder
      WPCmd set authpage [WPPercentQuote "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/open.tcl?folder=${folder}&colid=${colid}"]
      WPCmd set authcancel [WPPercentQuote "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders"]

      set src [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_queryauth.tcl]

    } else {
      catch {WPCmd PEInfo statmsg "Cannot open $path: $reason"}
      set src folders
    }
  } else {

    # manage caching last folder opened
    if {0 == [catch {WPCmd set wp_open_folder} last_folder]} {
      WPTFAddFolderCache [lindex $last_folder 0] [lindex $last_folder 1]
    }
    
    catch {WPCmd set wp_open_folder [list $colid $folder]}

    # start with the message indicated by the
    # incoming-startup-rule' in the current index
    set firstmsg 1
    if {![catch {WPCmd PEMailbox firstinteresting} firstint] && $firstint > 0} {
      set messagecount [WPCmd PEMailbox messagecount]
      if {[catch {WPCmd PEInfo indexlines} ppg] || $ppg == 0} {
	set ppg $_wp(indexlines)
      }

      for {set i 1} {$i < $messagecount} {incr i $ppg} {
	if {$i >= $firstint} {
	  break
	}

	set firstmsg $i
      }

      # show whole last page
      if {$firstmsg + $ppg > $messagecount} {
	if {[set n [expr {($messagecount + 1) - $ppg}]] > 0} {
	  set firstmsg $n
	} else {
	  set firstmsg 1
	}
      }
    }

    if {[catch {WPCmd PEMailbox uid $firstmsg} exp]} {
      set exp 1
    }

    WPCmd set first $firstmsg
    WPCmd set top $exp
    WPCmd set uid $exp
	  
    WPCmd set width $_wp(width)
    WPCmd set wp_spec_script fr_index.tcl
    set src main.tcl
  }
}

if {[info exists src]} {
  source [WPTFScript $src]
}

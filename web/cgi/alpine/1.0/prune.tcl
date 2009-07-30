# $Id: prune.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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
#  Purpose:  CGI script to perform monthly message pruning via prunetime.tcl
#	     generated form

#  Input: 
set prune_vars {
  {cid		"Missing Command ID"}
  {mvcnt	"Missing Move Count"}
  {delList	"" ""}
}

#  Output: 
#

## read vars
foreach item $prune_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {$cid != [WPCmd PEInfo key]} {
  error [list _action Postpone "Invalid Operation ID" "Click Back button to try again."]
}

set mvvals {}
for {set i 0} {$i < $mvcnt} {incr i} {
  WPLoadCGIVarAs "mv${i}" tmpmv
  if {[string compare $tmpmv ""]} {
    lappend mvvals $tmpmv
  }
}

foreach mvval $mvvals {
  set mvfrm [lindex $mvval 1]
  set mvto [lindex $mvval 2]

  if {[catch {WPCmd PEFolder rename default $mvfrm $mvto} result]} {
    set msg "Can't Rename $mvfrm: $result"
  } else {
    set msg "Renaming \"${mvfrm}\" at start of month"
    catch {WPCmd PEFolder create default $mvfrm} result
  }
  WPCmd PEInfo statmsg $msg
}

foreach delfldr $delList {
  set msg ""
  if {[catch {WPCmd PEFolder delete default $delfldr} result]} {
    set msg "Can't delete ${delfldr}: $result"
  } else {
    set msg "Deleted $delfldr"
  }
  WPCmd PEInfo statmsg $msg
}

source [WPTFScript main]

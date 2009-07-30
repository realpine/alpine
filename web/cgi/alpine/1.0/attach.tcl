# $Id: attach.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  attach.tcl
#
#  Purpose:  CGI script to handle attaching attachment
#	     to composition via queryattach generated form
#
#  Input: 
set attach_vars {
  {cid		"Missing Command ID"}
  {file		"Missing File Upload Data"}
  {attachop	""	""}
  {cancel	""	""}
}

#  Output: 
#

## read vars
foreach item $attach_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      error [list _action "Impart Variable" $result]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {$cancel == 1 || [string compare cancel [string tolower $cancel]] == 0 || [string compare cancel [string tolower $attachop]] == 0} {
} else {

  if {$cid != [WPCmd PEInfo key]} {
    catch {WPCmd PEInfo statmsg "Invalid Command ID"}
  }

  if {[string length [lindex $file 1]]} {
    if {[catch {cgi_import description}]} {
      set description ""
    }

    # "file" is a list: local_file remote_file content-type/content-subtype
    # trim path from file name on remote system
    # since we can't be certain what the delimiter is,
    # try the usual suspects
    set delims [list "\\" "/" ":"]
    set native [lindex $file 1]
    if {[string length $native]} {
      foreach delim $delims {
	if {[set crop [string last $delim $native]] >= 0} {
	  set native [string range $native [expr {$crop + 1}] [string length $native]]
	  break;
	}
      }

      regsub -all "'" $native "\\'" jsnative

      if {0 == [string length [lindex $file 2]]} {
	set conttype [list text plain]
      } else {
	set conttype [split [lindex $file 2] "/"]
      }

      set id [WPCmd PECompose attach [lindex $file 0] [lindex $conttype 0] [lindex $conttype 1] $native $description]

      catch {unset style}
      set restore 1

      if {[catch {WPCmd PEInfo lappend suspended_composition [list attach $id]} result]} {
	WPCmd PEInfo statmsg "Cannot append attachment info, nothing attached"
      }

      set fsize [file size [lindex $file 0]]
      if {$fsize <= 0} {
	WPCmd PEInfo statmsg "Attachment $jsnative empty or nonexistant"
      }
    } else {
      WPCmd PEInfo statmsg "Requested attachment does not exist"
    }
  } else {
    catch {file delete [lindex $file 0]}
    WPCmd PEInfo statmsg "Empty file name, nothing attached"
  }
}

source [WPTFScript compose]

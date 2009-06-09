#!./tclsh
# $Id: apply.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
# ========================================================================
# Copyright 2008 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

#  apply
#
#  Purpose:  CGI script generating response to xmlHttpRequest
#
#  Input:    
#            
set apply_args {
  {f	{}	""}
  {s	{}	""}
}

# inherit global config
source ../alpine.tcl


# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $apply_args {
  if {[catch {eval WPImport $item} errstr]} {
    WPInfoPage "Web Alpine Error" [font size=+2 $errstr] "Please close this window."
    exit
  }
}

switch -- $f {
  {new} -
  {imp} -
  {del} {
    set flag $f
  }
  default {
  }
}

switch -- $s {
  ton -
  not {
    set state $s
  }
  default {
  }
}

cgi_puts "Content-type: text/html; charset=\"UTF-8\""
cgi_puts ""
if {[catch {
  if {[info exists flag] && [info exists state]} {
    if {[catch {WPCmd PEMailbox apply flag $state $flag} result]} {
      error "Apply $state ${flag}: $result"
    } else {
      set response "$result [WPCmd PEMailbox selected] [WPCmd PEMailbox messagecount]"
      cgi_puts $response
    }
  } else {
    WPCmd PEInfo statmsg "Unknown flag ($f) or state ($s)"
  }
} result]} {
  cgi_puts "failed: $result"
}

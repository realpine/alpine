#!./tclsh
# $Id: take.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  take.tcl
#
#  Purpose:  CGI script to supply data to YUI AutoTake
#
#  Input:
#            along with possible search parameters:
set take_args {
  {op	{}	"noop"}
  {u	{}	0}
}

# inherit global config
source ../alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  error "take.tcl $result"
}

# grok parameters
foreach item $take_args {
  if {[catch {eval WPImport $item} result]} {
    set errstr $result
    break;
  }
}

switch -- $op {
  from {
    if {[catch {WPCmd PEMessage $u takefrom} result]} {
      set errstr $result
    }
  }
  all {
    if {[catch {WPCmd PEMessage $u takeaddr} result]} {
      set errstr $result
    }
  }
  default {
    set errstr "Unrecognized Take option: $op"
  }
}

puts stdout "Content-type: text/xml; charset=\"UTF-8\"\n"
puts stdout {<?xml version="1.0" encoding="UTF-8"?>}
puts stdout "<ResultSet totalResultsAvailable=\"[llength $result]\">"
if {[info exists errstr]} {
    puts -nonewline stdout "<Result><Error>$errstr</Error></Result>"
} else {
  foreach r $result {
    set ar [lindex $r 1]
    set sr [lindex $r 2]
    if {[string length [lindex $sr 0]] > 0
	|| [string length [lindex $sr 2]] > 0
	|| [string length [lindex $sr 3]] > 0} {
      set type "edit" 
    } else {
      set type "add"
    }
    # there is also an addrbook Fullname in sr 1
    puts -nonewline stdout "<Result>"
    puts -nonewline stdout "<Type>[cgi_quote_html $type]</Type>"
    puts -nonewline stdout "<Nickname>[cgi_quote_html [lindex $sr 0]]</Nickname>"
    puts -nonewline stdout "<Personal>[cgi_quote_html [lindex $ar 0]]</Personal>"
    puts -nonewline stdout "<Email>[cgi_quote_html "[lindex $ar 1]@[lindex $ar 2]"]</Email>"
    puts -nonewline stdout "<Fcc>[cgi_quote_html [lindex $sr 2]]</Fcc>"
    puts -nonewline stdout "<Note>[cgi_quote_html [lindex $sr 3]]</Note>"
    puts stdout "</Result>"
  }
}
puts stdout {</ResultSet>}

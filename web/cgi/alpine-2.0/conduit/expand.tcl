#!./tclsh
# $Id: expand.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  expand.tcl
#
#  Purpose:  CGI script to supply data to composer
#
#  Input:
#            along with possible search parameters:
set expand_args {
  {book		{}	-1}
  {index	{}	-1}
  {addrs		{}	""}
}

# inherit global config
source ../alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid} result]} {
  error "expand.tcl: $result"
}

# grok parameters
foreach item $expand_args {
  if {[catch {eval WPImport $item} result]} {
    error "expand.tcl: $result"
  }
}

catch (unset errstr}
set errlist {}
set expaddr {}

if {[regexp {^[0-9]*$} $book] && [regexp {^[0-9,]*$} $index]} {
  foreach i [split $index ","] {
    if {[catch {WPCmd PEAddress entry $book "" $i} entry]} {
      lappend errlist $entry
    } else {
      regsub -all "'" [lindex $entry 0] "\\'" resaddr
      lappend expaddr $resaddr
    }
  }
} elseif {[string length $addrs]} {
  if {[catch {WPCmd PEAddress expand $addrs [lindex [WPCmd PECompose fccdefault] 1]} result]} {
    set errstr $result
  } else {
    set expaddr [list [lindex $result 0]]
  }
} else {
  set errstr "Unknown expand options"
}

puts stdout "Content-type: text/xml; charset=\"UTF-8\"\n"
puts stdout {<?xml version="1.0" encoding="UTF-8"?>}

if {[info exists errstr] && [string length $errstr]} {
  puts stdout "<ResultSet totalResultsAvailable=\"1\"><Result>"
  puts stdout "<Error>$errstr</Error>"
  puts stdout "</Result></ResultSet>"
} else {
  puts stdout "<ResultSet totalResultsAvailable=\"[expr {[llength $expaddr] + [llength $errlist]}]\">"
  foreach e $errlist {
    puts stdout "<Result><Error>[cgi_quote_html $e]</Error></Result>"
  }

  foreach a $expaddr {
    puts stdout "<Result><Address>[cgi_quote_html $a]</Address></Result>"
  }

  puts stdout "</ResultSet>"
}

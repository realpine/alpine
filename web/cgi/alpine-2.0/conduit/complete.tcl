#!./tclsh
# $Id: complete.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  complete
#
#  Purpose:  CGI script to supply data to YUI AutoComplete
#
#  Input:
#            along with possible search parameters:
set complete_args {
  {param	{}	""}
  {query	{}	""}
  {uid		{}	0}
}

# inherit global config
source ../alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $complete_args {
  if {[catch {eval WPImport $item} errstr]} {
    error $errstr
  }
}

if {[catch {WPCmd PEAddress complete $query $uid} result]} {
  error "complete: $result"
}

puts stdout "Content-type: text/xml; charset=\"UTF-8\""
puts stdout ""
puts stdout {<?xml version="1.0" encoding="UTF-8"?>}
puts stdout "<ResultSet totalResultsAvailable=\"[llength $result]\">"
if {[string length $query]} {
  foreach abe $result {
    puts stdout "<Result><Nickname>[cgi_quote_html [lindex $abe 0]]</Nickname><Email>[cgi_quote_html [lindex $abe 1]]</Email></Result>"
  }
}
puts stdout {</ResultSet>}

#!./tclsh
# $Id: getcontact.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  getcontact.tcl
#
#  Purpose:  CGI script to handle saving new/edited contacts
#
#  Input: 
set contact_vars {
  {book		""	0}
  {index	""	-1}
}

#  Output: 

# inherit global config
source ../alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid} result]} {
  error "getcontact.tcl $result"
}

# grok parameters
foreach item $contact_vars {
  if {[catch {eval WPImport $item} result]} {
    error "getcontact.tcl $result"
  }
}

if {[catch {WPCmd PEAddress fullentry $book "" $index} addrinfo]} {
  error "getcontact.tcl $addrinfo"
}

puts stdout "Content-type: text/xml; charset=\"UTF-8\"\n"
puts stdout {<?xml version="1.0" encoding="UTF-8"?>}
puts stdout "<ResultSet totalResultsAvailable=\"1\"><Result>"
puts stdout "<Nickname>[cgi_quote_html [lindex $addrinfo 0]]</Nickname>"
puts stdout "<Personal>[cgi_quote_html [lindex $addrinfo 1]]</Personal>"
puts stdout "<Mailbox>[cgi_quote_html [join [lindex $addrinfo 2] ", "]]</Mailbox>"
puts stdout "<Fcc>[cgi_quote_html [lindex $addrinfo 3]]</Fcc>"
puts stdout "<Note>[cgi_quote_html [lindex $addrinfo 4]]</Note>"
puts stdout "</Result></ResultSet>"

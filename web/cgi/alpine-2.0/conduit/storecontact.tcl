#!./tclsh
# $Id: storecontact.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  storecontact.tcl
#
#  Purpose:  CGI script to handle saving new/edited contacts
#
#  Input: 
set store_vars {
  {book		""	0}
  {ai		""	-1}
  {contactNick	""	""}
  {contactName	""	""}
  {contactEmail	""	""}
  {contactFcc	""	""}
  {contactNotes	""	""}
}

#  Output: 

# inherit global config
source ../alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $store_vars {
  if {[catch {eval WPImport $item} errstr]} {
    lappend errs $errstr
  }
}

if {[string length $contactNick] || [string length $contactName] || [string length $contactEmail]} {
  set result ""
  if {[catch {WPCmd PEAddress edit $book $contactNick $ai $contactName $contactEmail $contactFcc $contactNotes 1} result]} {
    lappend status "Address Set Failure: $result"
  } elseif {[string length $result]} {
    lappend status "$result"
  } else {
    lappend status "Contact Added"
  }
} else {
  lappend status "No Contact Added: Must contain Display Name, Address or Nickname"
}

# return response text
puts stdout "Content-type: text/xml; charset=\"UTF-8\"\n"
puts stdout {<?xml version="1.0" encoding="UTF-8"?>}
puts stdout "<ResultSet totalResultsAvailable=\"[llength $status]\">"
foreach sm $status {
  regsub {'} $sm {\'} sm
  puts stdout "<Result><StatusText>$sm</StatusText></Result>"
}
puts stdout "</ResultSet>"

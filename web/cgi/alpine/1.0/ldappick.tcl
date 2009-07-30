# $Id: ldappick.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  ldappick.tcl
#
#  Purpose:  CGI script to handle LDAP result choices from the
#	     via the LDAP result browser generated form

#  Input: 
set pick_vars {
  {cid		"Missing Command ID"}
  {field	"Missing Field Name"}
  {ldapquery	"Missing LDAP Query"}
  {ldapList	""	""}
  {addresses	""	""}
  {addrop	{}	""}
  {cancel	{}	0}
}

#  Output: 
#

# read vars
foreach item $pick_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      error [list _action "Impart Variable" $result]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {$cid != [WPCmd PEInfo key]} {
  catch {WPCmd PEInfo statmsg "Invalid Command ID"}
}

if {$cancel != 1 && !([string compare cancel [string tolower $cancel]] == 0 || [string compare cancel [string tolower $addrop]] == 0) && [string length $ldapList] > 0} {
  set ldapListStr [join $ldapList ","]
  if {[catch {WPCmd PELdap setaddrs $ldapquery $ldapListStr $addresses 0} newaddrlist]} {
    WPCmd PEInfo statmsg "LDAP Error: $newaddrlist"
  } else {
    regsub -all "'" $newaddrlist "\\'" newaddrs

    if {[catch {WPCmd PEInfo set suspended_composition} msgdata]} {
      WPCmd PEInfo statmsg "Cannot read message data: $msgdata"
    } else {
      if {[info exists newaddrs]} {
	for {set i 0} {$i < [llength $msgdata]} {incr i} {
	  set orig_field [lindex [lindex $msgdata $i] 0]
	  regsub -all -- - [string tolower $orig_field] _ fn

	  if {[string compare $fn $field] == 0} {
	    set msgdata [lreplace $msgdata $i $i [list $orig_field $newaddrs]]
	    break
	  }
	}

	if {[catch {WPCmd PEInfo set suspended_composition $msgdata} result]} {
	  WPCmd PEInfo statmsg "Cannot Update $field field: $result"
	}
      }
    }
  }
}

source [WPTFScript compose]

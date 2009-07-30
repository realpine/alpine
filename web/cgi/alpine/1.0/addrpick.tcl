# $Id: addrpick.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  addrpick.tcl
#
#  Purpose:  CGI script to handle address book choices from the
#	     via the addrbook browser generated form
#
#  Input: 
set pick_vars {
  {cid		"Missing Command ID"}
  {field	"Missing Field Name"}
  {addrop	{}	""}
  {cancel	{}	0}
}

#  Output: 


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

WPLoadCGIVarAs nickList listonicks
set usenewfcc 0

if {$cancel == 1 || [string compare cancel [string tolower $cancel]] == 0 || [string compare cancel [string tolower $addrop]] == 0} {
} elseif {[string length $listonicks] > 0} {
  foreach nick $listonicks {
    # determine which book
    if {[regexp {^([0-9]+)\.([0-9]+)\.(.*)$} $nick dummy book ai nick]} {
      if {[catch {WPCmd PEAddress entry $book $nick $ai} result]} {
	regsub -all "'" $result "\\'" result
	WPCmd PEInfo statmsg $result
      } else {
	set resaddr [lindex $result 0]
	regsub -all "'" $resaddr "\\'" resaddr
	if {[info exists newaddrs]} {
	  append newaddrs ", "
	}
	if {[string compare $field "to"] == 0 && [string length [lindex $result 1]]} {
	  # arbitrarily the last returned entry with an fcc wins
	  set fcc [lindex $result 1]
	  if {[string compare $fcc "\"\""] == 0} {
	    set fcc ""
	  }
	  regsub -all "'" $fcc "\\'" fcc
	  set usenewfcc 1
	}

	append newaddrs $resaddr
      }
    } else {
      WPCmd PEInfo statmsg "Malformed entry request: $addr"
    }
  }

  if {[info exists newaddrs]} {
    if {[catch {WPCmd PEInfo set suspended_composition} msgdata] == 0} {
      for {set i 0} {$i < [llength $msgdata]} {incr i} {
	if {[string compare [string tolower [lindex [lindex $msgdata $i] 0]] $field] == 0} {
	  if {[string length [lindex [lindex $msgdata $i] 1]]} {
	    set newfield [list $field "[lindex [lindex $msgdata $i] 1], $newaddrs"]
	  } else {
	    set newfield [list $field $newaddrs]
	  }
	  break
	}
      }

      if {$usenewfcc} {
	for {set j 0} {$j < [llength $msgdata]} {incr j} {
	  if {[string compare fcc [string tolower [lindex [lindex $msgdata $j] 0]]] == 0} {
	    set fcc_index $j
	    break
	  }
	}

	set savedef [WPTFSaveDefault 0]
	set colid [lindex $savedef 0]
	if {[info exists fcc_index]} {
	  if {[string compare $fcc [lindex [lindex [lindex $msgdata $fcc_index] 1] 1]]} {
	    lappend msgdata [list postoption [list fcc-set-by-addrbook 1]]
	  }

	  set msgdata [lreplace $msgdata $fcc_index $fcc_index [list Fcc [list $colid $fcc]]]
	} else {
	  lappend msgdata [list Fcc [list $colid $fcc]]
	  lappend msgdata [list postoption [list fcc-set-by-addrbook 1]]
	}
      }

      if {[info exists newfield]} {
	set msgdata2 [lreplace $msgdata $i $i $newfield]
	if {[catch {WPCmd PEInfo set suspended_composition $msgdata2} result] == 0} {
	  unset result
	}
      } else {
	lappend msgdata [list $field $newaddrs]
	if {[catch {WPCmd PEInfo set suspended_composition $msgdata} result] == 0} {
	  unset result
	}
      }

      if {[info exists result]} {
	WPCmd PEInfo statmsg "Cannot Update $field field: $result"
      }
    } else {
      WPCmd PEInfo statmsg "Cannot change Message Data: $msgdata"
    }
  }
}

source [WPTFScript compose]

# $Id: addrsave.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  addrsave.tcl
#
#  Purpose:  CGI script to handle address book change/save
#	     via addredit generated form
#
#  Input: 
set abs_vars {
  {cid		"Missing Command ID"}
  {save		{}	0}
  {delete	{}	0}
  {replace	{}	0}
  {compose	{}	0}
  {take		{}	0}
  {help		{}	0}
  {cancel	{}	0}
  {oncancel	{}	""}
}

#  Output: 
#

## read vars
foreach item $abs_vars {
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

if {$help != 0} {
  set save_vars {
    {book {} 0}
    {newnick {} ""}
    {nick {} ""}
    {ai {} 0}
    {fn {} ""}
    {addrs {} ""}
    {fcc {} ""}
    {comment {} ""}
    {add {} 0}
    {take {} 0}
  }

  foreach i $abs_vars {
    lappend ae_help_state [list [lindex $i 0] [subst $[lindex $i 0]]]
  }

  foreach i $save_vars {
    eval WPImport $i
    lappend ae_help_state [list [lindex $i 0] [subst $[lindex $i 0]]]
  }
  
  WPCmd PEInfo set ae_help_state $ae_help_state

  set _cgi_uservar(topic) takeedit
  set _cgi_uservar(oncancel) addredit
  if {$take} {
    set _cgi_uservar(index) none
  }

  set src help

} elseif {$save == 1 || [string compare [string tolower $save] "save entry"] == 0} {
  WPLoadCGIVar book
  WPLoadCGIVar newnick
  WPLoadCGIVar ai
  WPLoadCGIVar fn
  WPLoadCGIVar addrs
  WPLoadCGIVar fcc
  WPLoadCGIVar comment
  if {[catch {cgi_import_cookie add}] != 0} {set add 0}
  if {[catch {cgi_import_cookie nick}] != 0} {set nick ""}

  if {![string length $newnick]} {
    WPCmd PEInfo statmsg "No entry created. New entries must include a Nickname."
    set src takeedit
  } else {
    if {!$take || [catch {WPCmd PEAddress entry $book $newnick $ai} result] || ![llength $result]} {
      set result ""
      if {[catch {WPCmd PEAddress edit $book $newnick $ai $fn $addrs $fcc $comment $add $nick} result]} {
	catch {WPCmd PEInfo statmsg "Address Set Failure: $result"}
	set adderr $result
      } elseif {[string length $result]} {
	catch {WPCmd PEInfo statmsg "$result"}
	set adderr $result
      }

      if {$take == 1} {
	if {[info exists adderr]} {
	  WPCmd PEInfo statmsg "No address book entry changed: $adderr"
	} else {
	  WPCmd PEInfo statmsg "New address book entry \"$newnick\" created."
	}
      }
    } else {
      set src takesame
    }

    if {$take == 1 && ![info exists src]} {
      set src $oncancel
    }
  }
} elseif {$delete == 1 || [string compare [string tolower $delete] "delete entry"] == 0} {
  WPLoadCGIVar book
  WPLoadCGIVar nick
  WPLoadCGIVar ai

  set result ""
  if {[catch {WPCmd PEAddress delete $book $nick $ai} result]} {
    catch {WPCmd PEInfo statmsg "Address Set Failure $result"}
  } elseif {[string compare $result ""]} {
    catch {WPCmd PEInfo statmsg "$result"}
  }
} elseif {[string compare [string tolower $replace] "replace entry"] == 0} {

  WPLoadCGIVar book
  WPLoadCGIVar newnick
  WPLoadCGIVar ai
  WPLoadCGIVar fn
  WPLoadCGIVar addrs
  WPLoadCGIVar fcc
  WPLoadCGIVar comment
  if {[catch {cgi_import_cookie add}] != 0} {set add 0}
  if {[catch {cgi_import_cookie nick}] != 0} {set nick ""}

  if {![string length $newnick]} {
    WPCmd PEInfo statmsg "No entry created. New entries must include a Nickname."
    set src takeedit
  } else {
    if {[catch {WPCmd PEAddress delete $book $newnick $ai} result]} {
      set adderr $result
    } else {
      set result ""
      if {[catch {WPCmd PEAddress edit $book $newnick $ai $fn $addrs $fcc $comment $add $nick} result]} {
	catch {WPCmd PEInfo statmsg "Address Set Failure result"}
	set adderr $result
      } elseif {[string length $result]} {
	catch {WPCmd PEInfo statmsg "$result"}
	set adderr $result
      }
    }

    if {$take == 1} {
      if {[info exists adderr]} {
	WPCmd PEInfo statmsg "No address book entry changed: $adderr."
      } else {
	WPCmd PEInfo statmsg "Address book entry \"$newnick\" replaced."
      }

      if {![info exists src]} {
	set src $oncancel
      }
    }
  }
} elseif {[string compare [string tolower $replace] "add to entry"] == 0} {
  WPLoadCGIVar book
  WPLoadCGIVar newnick
  WPLoadCGIVar ai
  WPLoadCGIVar fn
  WPLoadCGIVar addrs
  WPLoadCGIVar fcc
  WPLoadCGIVar comment
  if {[catch {cgi_import_cookie add}] != 0} {set add 0}
  if {[catch {cgi_import_cookie nick}] != 0} {set nick ""}

  if {![string length $newnick]} {
    WPCmd PEInfo statmsg "No entry created. New entries must include a Nickname."
    set src takeedit
  } else {
    if {[catch {WPCmd PEAddress fullentry $book $newnick $ai} result] || ![llength $result]} {
      if {[string length $result]} {
	set adderr $result
      } else {
	set adderr "No pre-existing entry"
      }
    } else {
      set fn [lindex $result 1]
      set newaddrs [join [lindex $result 2] ","]
      append newaddrs ", $addrs"
      set fcc [lindex $result 3]
      set comment [lindex $result 4]
      set result ""
      if {[catch {WPCmd PEAddress edit $book $newnick $ai $fn $newaddrs $fcc $comment 0 $newnick} result]} {
	set adderr $result
      } elseif {[string length $result]} {
	catch {WPCmd PEInfo statmsg "$result"}
	set adderr $result
      }
    }

    if {$take == 1} {
      if {[info exists adderr]} {
	WPCmd PEInfo statmsg "No address book entry created: $adderr."
      } else {
	WPCmd PEInfo statmsg "Address book entry \"$newnick\" appended"
      }

      if {![info exists src]} {
	set src $oncancel
      }
    }
  }
} elseif {[string compare [string tolower $replace] edit] == 0} {
  set src takeedit
} elseif {$compose == 1} {
  set oncancel addrbook
  set src compose
} elseif {$cancel == 1 || [string compare [string tolower $cancel] cancel] == 0} {
  if {$take == 1} {
    set act "Take Address"
  } else {
    set act "Address Edit"
  }

  catch {WPCmd PEInfo statmsg "$act cancelled.  Address book unchanged."}
  set src $oncancel
} else {
  catch {WPCmd PEInfo statmsg "Unknown Address Book Operation"}
}

if {![info exists src]} {
  set src addrbook
}

source [WPTFScript $src]

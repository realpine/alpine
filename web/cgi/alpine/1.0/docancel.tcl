# $Id: docancel.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  post.tcl
#
#  Purpose:  CGI script to perform message posting via compose.tcl
#	     generated form
#
#  Input: 
set post_vars {
  {cid		"Missing Command ID"}
  {postpost	""			main}
}

#  Output: 
#

## read vars
foreach item $post_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      error [list _action "Impart Variable" $result]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {$cid != [WPCmd PEInfo key]} {
  error [list _action Postpone "Invalid Operation ID" "Click Back button to try again."]
}

# clean up attachments
WPCmd PEInfo statmsg "Message cancelled"
catch {WPCmd PEInfo unset suspended_composition}

source [WPTFScript $postpost]

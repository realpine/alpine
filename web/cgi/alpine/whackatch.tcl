#!./tclsh

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

#  whackatch.tcl
#
#  Purpose:  CGI script to cleanup requested attachment

#  Input:
#   attachment handle 

#  Output:
#

# inherit global config
source ./alpine.tcl

# seconds to pause before rechecking for abandanded attachment files
set abandoned 60

if {[gets stdin handle] >= 0 && [regexp {^[A-Za-z0-9]+$} $handle handle] == 1} {

  set flist [file join $_wp(fileroot) $_wp(detachpath) detach.${handle}.control]
  lappend flist [file join $_wp(fileroot) $_wp(detachpath) detach.${handle}.data]

  for {set cleanup 0} {$cleanup == 0} {} {
    set timein [clock seconds]

    after [expr {$abandoned * 1000}]

    foreach f $flist {
      if {($timein - [file atime $f]) > $abandoned} {
	set cleanup 1
      } else {
	set cleanup 0
	break
      }
    }
  }

  foreach f $flist {
    catch {exec /bin/rm -f $f}
  }
}

#!./tclsh
# $Id: whackatch.tcl 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $
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
#   ext - attachment file extension

#  Output:
#

# inherit global config
source ./alpine.tcl

# seconds to pause before rechecking for abandanded attachment files
set abandoned 300

# no dots allowed
if {[gets stdin ext] >= 0 && [regexp {^[A-Za-z0-9\-]+$} $ext ext] == 1} {

  set towhack [file join $_wp(fileroot) $_wp(detachpath) detach.${ext}]

  while {1} {
    set timein [clock seconds]

    after [expr {$abandoned * 1000}]

    if {[catch {file atime $towhack} atime] || ($timein - $atime) > $abandoned} {
      break
    }
  }

  catch {exec /bin/rm -f $towhack}
}

#!./tclsh
# $Id: usage.tcl 1169 2008-08-27 06:42:06Z hubert@u.washington.edu $
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

#  Return mail store usage numbers on stdout separated by a space
#  First number is amount of usage
#  Second is total amount of space available
#  Integer values.  Unit are megabytes (MB).

set cmd "exec -- /usr/local/bin/dmq -u [lindex $argv 0]"
if {0 == [catch {eval $cmd} result]} {
  if {[regexp {^[0-9]+[ \t]+([0-9]+)\.[0-9]*[ \t]+([0-9]+)$} $result dummy usage total]} {
    puts stdout "$usage $total"
  }
}

#!./tclsh
# $Id: open.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  open.tcl
#
#  Purpose:  CGI script to perform folder opening via folders.tcl

#  Input: 
set open_vars {
  {cid		"Missing Command ID"}
  {oncancel	""	"folders"}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

WPEval $open_vars {
  source do_open.tcl
}

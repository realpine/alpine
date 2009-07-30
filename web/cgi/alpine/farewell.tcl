#!./tclsh
# $Id: farewell.tcl 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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


# Input:
set fw_vars {
  {serverid	{}	0}
  {verdir	{}	"/"}
}

# Output:
#

# global config
source ./alpine.tcl

WPEval $fw_vars {
  if {[catch {cgi_import logerr}] == 0} {
    set parms "?logerr=$logerr"
  } else {
    set parms ""
  }

  cgi_http_head {
    # clear cookies
    cgi_cookie_set sessid=0 expires=now path=[file join / $verdir]

    cgi_redirect $_wp(serverpath)/session/logout/logout.tcl${parms}
  }
}

#!./tclsh
# $Id: startup.tcl 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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

# read config
source ./alpine.tcl

# Input: page

# Output: redirection to given page

cgi_eval {
  cgi_input

  if {[catch {cgi_import page}]} {
      WPInfoPage "Bogus Page Request" \
	  "[font size=+2 "Invalid Page Request!"]" \
	  "Please click your browser's [bold Back] button to return to the [cgi_link Start]"
  } else {
    cgi_http_head {
      cgi_redirect $page
    }
  }
}
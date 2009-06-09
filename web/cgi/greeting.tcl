#!./tclsh

# ========================================================================
# Copyright 2006-2007 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

source ./alpine.tcl

cgi_eval {

  cgi_http_head {
    cgi_redirect [cgi_root]/session/greeting.tcl
  }
}

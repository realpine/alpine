#!./tclsh
# $Id: fr_view.tcl 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  fr_view.tcl - wrapper around do_view.tcl to start tcl interp
#
#  Purpose:  CGI script to serve as the frame-work for including
#	     supplied script snippets that generate the various 
#	     javascript-free webpine pages

#  Input:

#  Output:
#

# inherit global config
source ./alpine.tcl

WPEval {} {
  source do_view.tcl
}

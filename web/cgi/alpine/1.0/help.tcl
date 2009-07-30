#!./tclsh
# $Id: help.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  help.tcl
#
#  Purpose:  CGI script to generate html help text for Alpine

#  Input:
set help_vars {
  {topic	{}	""}
  {topicclass	{}	""}
  {index	{}	""}
  {oncancel	{}	main}
  {params	{}	""}
}

#  Output:
#
#	HTML/Javascript/CSS help text for alpine

# inherit global config
source ./alpine.tcl

WPEval $help_vars {
  source do_help.tcl
}

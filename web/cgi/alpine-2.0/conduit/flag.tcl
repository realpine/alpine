#!./tclsh
# $Id: flag.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  flag
#
#  Purpose:  CGI script generating response to xmlHttpRequest
#
#  Input:    
#            
set flag_args {
  {u	{}	""}
  {f	{}	""}
  {s	{}	""}
}

# inherit global config
source ../alpine.tcl

WPEval $flag_args {
  cgi_body {
    switch -- $f {
      {new} -
      {imp} {
	set flag $f
      }
      default {
      }
    }

    switch -- $s {
      ton -
      not {
	set state $s
      }
      default {
      }
    }

    if {[info exists flag] && [info exists state]} {
      regsub -all {,} $u { } u
      if {[regexp {^[ 0123456789]*$} $u]} {
	switch $state {
	  ton { set state 1 }
	  not { set state 0 }
	}
	switch $flag {
	  imp { set flag important }
	}

	foreach eu $u {
	  if {[catch {WPCmd PEMessage $eu flag $flag $state} result]} {
	    set result "FALURE: setting $eu to $setting : $result"
	    break
	  }
	}
      } elseif {0 == [string compare $u all]} {
	if {[catch {WPCmd PEMailbox flag $state $flag} result]} {
	  set result "FAILURE: flag $state $flag : $result"
	}
      }
    } else {
      set result "FAILURE: unkown flag ($f) or state ($s)"
    }

    cgi_puts $result
  }
}

#!./tclsh
# $Id: query.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  query.tcl
#
#  Purpose:  CGI script to handle querying LDAP directory
#
#  Input: 
set query_vars {
  {dir		""	"0"}
  {query	""	""}
}

#  Output: 

# inherit global config
source ../alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $query_vars {
  if {[catch {eval WPImport $item} errstr]} {
    lappend errs $errstr
  }
}

set qresult ""

# return attachment list
puts stdout "Content-type: text/html;\n\n<html><head><script>window.parent.drawLDAPResult({"
if {[string length $query]} {
  if {[catch {WPCmd PELdap query $dir $query ""} qn]} {
    regsub -all {'} $qn {\'} qn
    puts stdout "error:'Search failed: $qn'"
  } else {
    switch $qn {
      0 {  puts stdout "error:'Search found no matching entries'" }
      default {
	if {[catch {WPCmd PELdap results $qn} results]} {
	  regsub -all {'} $results {\'} results
	  puts stdout "error:'Problem with results: $results'" 
	} else {
	  puts stdout "results:\["
	  foreach result $results {
	    set res [lindex $result 1]
	    set comma ""
	    foreach r $res {
	      regsub -all {'} $r {\'} r
	      puts -nonewline stdout "${comma}{personal:'[lindex $r 0]',email:\["
	      set comma2 ""
	      foreach a [lindex $r 4] {
		puts -nonewline stdout "$comma2'$a'"
		set comma2 ","
	      }
	      puts -nonewline stdout "\]}"
	      set comma ","
	    }
	  }

	  puts stdout "\]"
	}
      }
    }
  }
} else {
  puts stdout "error:'Empty search request'"
}

puts stdout "});</script></head><body></body></html>"

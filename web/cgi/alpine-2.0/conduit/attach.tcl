#!./tclsh
# $Id: attach.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  attach.tcl
#
#  Purpose:  CGI script to handle attaching attachment
#	     to composition via queryattach generated form
#
#  Input: 
set attach_vars {
  {file		""	""}
  {description	""	""}
  {op		""	""}
  {id		""	0}
}

#  Output: 

# inherit global config
source ../alpine.tcl
cgi_debug -on
# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $attach_vars {
  if {[catch {eval WPImport $item} errstr]} {
    lappend errs $errstr
  }
}

if {[string length $file] && [string length [lindex $file 1]]} {

  # "file" is a list: local_file remote_file content-type/content-subtype
  # trim path from file name on remote system
  # since we can't be certain what the delimiter is,
  # try the usual suspects
  set delims [list "\\" "/" ":"]
  set native [lindex $file 1]
  if {[string length $native]} {
    foreach delim $delims {
      if {[set crop [string last $delim $native]] >= 0} {
	set native [string range $native [expr {$crop + 1}] [string length $native]]
	break;
      }
    }

    regsub -all "'" $native "\\'" jsnative

    if {0 == [string length [lindex $file 2]]} {
      set conttype [list text plain]
    } else {
      set conttype [split [lindex $file 2] "/"]
    }

    set id [WPCmd PECompose attach [lindex $file 0] [lindex $conttype 0] [lindex $conttype 1] $native $description]
  } else {
    lappend errs "Requested attachment does not exist"
  }
} elseif {![string compare delete $op]} {
  if {[catch {WPCmd PECompose unattach $id} result]} {
    lappend errs $result
  }
}



# return attachment list
puts stdout "Content-type: text/html;\n\n<html><head><script>window.parent.drawAttachmentList(\{"
if {0 == [catch {WPCmd PECompose attachments} attachments]} {
  puts stdout "attachments:\["
  set comma ""
  foreach a $attachments {
    # {4137457288 bunny.gif 2514 Image/GIF}
    puts stdout "${comma}\{id:\"[lindex $a 0]\",fn:\"[lindex $a 1]\",size:\"[lindex $a 2]\",type:\"[lindex $a 3]\"\}"
    set comma ","
  }
  puts stdout "\]"
  set comma ","
} else {
  set comma ""
  lappend errs $attachments
}

if {[info exists errs]} {
  puts stdout "${comma}error:\"[join $errs {, }]\""
}

puts stdout "\});</script></head><body></body></html>"

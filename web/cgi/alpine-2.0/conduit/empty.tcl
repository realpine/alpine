#!./tclsh
# $Id: empty.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  empty.tcl
#
#  Purpose:  CGI script generating response to xmlHttpRequest
#
#  Input:    
#            
set empty_args {
  {c		"Unspecified Collection"}
  {f		"Unspecified Folder"}
  {u		""	0}
}

# inherit global config
source ../alpine.tcl
source ../common.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $empty_args {
  if {[catch {eval WPImport $item} errstr]} {
    WPInfoPage "Web Alpine Error" [font size=+2 $errstr] "Please close this window."
    exit
  }
}


cgi_puts "Content-type: text/html; charset=\"UTF-8\""
cgi_puts ""

# ONLY ever empty Junk, Trash or Drafts
set defc [WPCmd PEFolder defaultcollection]

set tf [lindex [WPCmd PEConfig varget trash-folder] 0]
set df [lindex [WPCmd PEConfig varget postponed-folder] 0]
set f [wpLiteralFolder $c $f]
if {$c == $defc
    && (([info exists _wp(spamfolder)] && 0 == [string compare $f $_wp(spamfolder)])
	|| ([string length $tf] && 0 == [string compare $f $tf])
	|| ([string length $tf] && 0 == [string compare $f $df]))} {
  if {[catch {
    switch -regexp $u {
      ^([0-9]+)$ -
      ^selected$ -
      ^all$ {
	cgi_puts [WPCmd PEFolder empty $c $f $u]
      }
      default {
	error "Unknown option"
      }
    }
  } result]} {
    cgi_puts "$result"
  }
} else {
  cgi_puts "Empty $f NOT permitted"
}

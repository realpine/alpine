#!./tclsh
# $Id: newmail.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  newmail.tcl
#
#  Purpose:  CGI script generating response to xmlHttpRequest
#
#  Input:    
#            
set newmail_args {
  {reload	{}	0}
}

# inherit global config
source ../alpine.tcl


# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $newmail_args {
  if {[catch {eval WPImport $item} errstr]} {
    WPInfoPage "Web Alpine Error" [font size=+2 $errstr] "Please close this window."
    return
  }
}

cgi_puts "Content-type: text/html; charset=\"UTF-8\""
cgi_puts ""
if {[catch {WPCmd PEMailbox newmail $reload} newmail]} {
  cgi_puts "PROBLEM: $newmail"
} else {
  cgi_puts "$newmail"
}

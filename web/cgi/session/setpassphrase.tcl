#!./tclsh
# $Id: setpassphrase.tcl 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $
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

#  setpassphrase.tcl
#
#  Purpose:  CGI script to accept user passphrase
#            via xmlHttpRequest

#  Input:
set auth_vars {
  {auths	""	0}
  {pass		""	0}
  {cancel	""	0}
}

#  Output:
#

# inherit global config
source ./alpine.tcl

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $auth_vars {
  if {[catch {eval WPImport $item} errstr]} {
    WPInfoPage "Web Alpine Error" [font size=+2 $errstr] "Please close this window."
    return
  }
}

cgi_puts "Content-type: text/html; charset=\"UTF-8\"\n"
set answer "Problem setting passphrase"

if {[string compare $auths "Smime"] != 0
    || [string length $pass] == 0
    || [catch {WPCmd PESession setpassphrase $pass} answer]} {
  cgi_puts "Cannot accept passphrase: $answer"
}

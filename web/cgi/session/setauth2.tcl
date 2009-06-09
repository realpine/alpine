#!./tclsh
# $Id: setauth2.tcl 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  setauth2.tcl
#
#  Purpose:  CGI script to accept user authorization
#            credentials via xmlHttpRequest

#  Input:
set auth_vars {
  {c		"No Authorization Collection"}
  {f		"No Authorization Folder"}
  {auths	""	0}
  {user		""	0}
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
set answer "Problem setting authorization credentials"


if {[string compare $auths "Login"] == 0
    && [string length $user]
    && [catch {WPCmd PESession creds $c $f $user $pass} answer] == 0} {
  cgi_puts "$answer"
} else {
  cgi_puts "Cannot accept login: $answer"
}

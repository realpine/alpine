#!./tclsh
# $Id: setauth.tcl 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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

#  setauth.tcl
#
#  Purpose:  CGI script to generate html form used to ask for authentication
#            credentials

#  Input:
set auth_vars {
  {cid		"Missing Command ID"}
  {authcol	"No Authorization Collection"}
  {authfolder	"No Authorization Folder"}
  {authpage	"No Post Authorization Instructions"}
  {authcancel	"No Auth Cancel Instructions"}
  {auths	""	0}
  {user		""	0}
  {pass		""	0}
  {cancel	""	0}
}

#  Output:
#
#	Redirect to specified post-authentication page

# inherit global config
source ./alpine.tcl


WPEval $auth_vars {

  if {$cid != [WPCmd PEInfo key]} {
    error [list _action open "Invalid Operation ID" "Click Back button to try again."]
  }

  # if NOT cancelled
  if {[string compare $auths "Login"] == 0
      && [string length $user]
      && [catch {WPCmd PESession creds $authcol $authfolder $user $pass}] == 0} {
    set redirect $authpage
  } else {
    set redirect $authcancel
  }

  cgi_http_head {
    # redirect to the place we stuffed the export info.  use the ip address
    # to foil spilling any session cookies or the like

    if {[info exists env(SERVER_PROTOCOL)] && [regexp {[Hh][Tt][Tt][PP]/([0-9]+)\.([0-9]+)} $env(SERVER_PROTOCOL) m vmaj vmin] && $vmaj >= 1 && $vmin >= 1} {
      cgi_puts "Status: 303 Temporary Redirect"
    } else {
      cgi_puts "Status: 302 Redirected"
    }

    cgi_puts "URI: $redirect"
    cgi_puts "Location: $redirect"
  }
}

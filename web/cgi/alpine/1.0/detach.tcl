#!./tclsh
# $Id: detach.tcl 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $
# ========================================================================
# Copyright 2006-2007 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

#  detach.tcl
#
#  Purpose:  CGI script to retrieve requested attachment
#
#  Input:
set detach_vars {
  {uid		"Unknown Message UID"}
  {part		"Unknown Message Part"}
  {download	""	0}
}

#set detach_via_ip_address 1
#set detach_via_local_hostname 1

# inherit global config
source ./alpine.tcl

proc WPServerIP {} {
  global _wp

  catch {
    set ip 127.0.0.1
    set sid [socket -async [info hostname] [expr {([string length $_wp(serverport)]) ? $_wp(serverport) : 80}]]
    set ip  [lindex [ fconfigure $sid -sockname ] 0]
    close $sid
  }

  return $ip
}

WPEval $detach_vars {
  if {[info exists env(PATH_INFO)]} {
    if {[string index $env(PATH_INFO) 0] == "/"} {
      set s [string range $env(PATH_INFO) 1 end]
      if {[set i [string first "/" $s]] >= 0} {
	set uid [string range $s 0 [expr {$i - 1}]]
	set s [string range $s [incr i] end]
	if {[set i [string first "/" $s]] >= 0} {
	  set part [string range $s 0 [expr {$i - 1}]]
	}
      }
    }
  }

  if {[info exists uid] == 0 || [info exists part] == 0} {
    error [list _action "Unspecified attachment UID or Part" "Please close this window."]
  }

  # generate big random string to reference the thing

  # generate filenames to hold detached data and control file
  for {set n 0} {1} {incr n} {

    set rhandle [WPCmd PESession random 64]
    set cfile [file join $_wp(fileroot) $_wp(detachpath) detach.${rhandle}-control]
    set dfile [file join $_wp(fileroot) $_wp(detachpath) detach.${rhandle}-data]

    if {[file exists $cfile] == 0 && [file exists $dfile] == 0} {
      if {[catch {open $cfile {RDWR CREAT EXCL} [cgi_tmpfile_permissions]} cfd]} {
	error [list _action Detach "Cannot create control file: [cgi_quote_html $cfd]" "Please close this window"]
      } else {
	exec echo ${rhandle}-control | [file join $_wp(cgipath) $_wp(appdir) whackatch.tcl] >& /dev/null &
      }

      if {[catch {open $dfile {RDWR CREAT EXCL} [cgi_tmpfile_permissions]} dfd]} {
	catch {close $cfd}
	error [list _action Detach "Cannot create command file: [cgi_quote_html $dfd]" "Please close this window"]
      } else {
	exec echo ${rhandle}-data | [file join $_wp(cgipath) $_wp(appdir) whackatch.tcl] >& /dev/null &
      }

      # exec chmod [cgi_tmpfile_permissions] $cfile
      # exec chmod [cgi_tmpfile_permissions] $dfile
      break
    } elseif {$n > 4} {
      error [list _action Detach "Command file creation limit" "Please close this window"]
    }
  }

  if {[catch {WPCmd PEMessage $uid detach $part $dfile} attachdata]} {
    error [list _action Detach $attachdata "Please close this window"]
  }

  if {[info exists detach_via_ip_address]} {
    if {[regsub {^(http[s]?://)[A-Za-z0-9\\-\\.]+(.*)$} "[cgi_root]/pub/getach.tcl" "\\1\[[WPServerIP]\]\\2" redirect] != 1} {
      error [list _action Detach "Cannot determine server address" "Please close this window"]
    }
  } elseif {[info exists detach_via_local_hostname]} {
    if {[regsub {^(http[s]?://)[A-Za-z0-9\\-\\.]+(.*)$} "[cgi_root]/pub/getach.tcl" "\\1\[[info hostname]\]\\2" redirect] != 1} {
      error [list _action Detach "Cannot determine server address" "Please close this window"]
    }
  } else {
    set redirect "[cgi_root]/pub/getach.tcl"
  }

  set mimetype [lindex $attachdata 0]
  set mimesubtype [lindex $attachdata 1]
  set contentlength [lindex $attachdata 2]
  set givenname [lindex [lindex $attachdata 3] 0]
  set tmpfile [lindex $attachdata 4]

  if {[string compare $tmpfile $dfile]} {
    set straytmp "&straytmp=1"
  } else {
    set straytmp ""
  }

  if {![string length $givenname]} {
    set givenname "attachment"
    switch -regexp $mimetype {
      ^[Tt][Ee][Xx][Tt]$ {
	switch -regexp $mimesubtype {
	  ^[Pp][Ll][Aa][Ii][Nn]$ {
	    set givenname "attached.txt"
	  }
	  ^[Hh][Tt][Mm][Ll]$ {
	    set givenname "attached.html"
	  }
	}
      }
    }
  }

  set safegivenname $givenname
  regsub -all {[/]} $safegivenname {-} safegivenname
  regsub -all {[ ]} $safegivenname {_} safegivenname
  regsub -all {[\?]} $safegivenname {X} safegivenname
  regsub -all {[&]} $safegivenname {X} safegivenname
  regsub -all {[#]} $safegivenname {X} safegivenname
  regsub -all {[=]} $safegivenname {X} safegivenname
  set safegivenname "/[WPPercentQuote $safegivenname {.}]"

  if {$download == 1} {
    puts $cfd "Content-type: Application/X-Download"
    puts $cfd "Content-Disposition: attachment; filename=\"$givenname\""
  } else {
    puts $cfd "Content-type: ${mimetype}/${mimesubtype}"
  }

  # side-step the cgi_xxx stuff in this special case because
  # we don't want to buffer up the downloading attachment...

  puts $cfd "Content-Length: $contentlength"
  puts $cfd "Expires: [clock format [expr {[clock seconds] + 3600}] -f {%a, %d %b %Y %H:%M:%S GMT} -gmt true]"
  puts $cfd "Cache-Control: max-age=3600"
  puts $cfd ""

  puts $cfd $tmpfile

  # exec chmod [cgi_tmpfile_permissions] $tmpfile

  close $cfd

  # prepare to clean up if the brower never redirects

  cgi_http_head {
    # redirect to the place we stuffed the detach info.  use the ip address
    # to foil spilling any session cookies or the like
    #cgi_redirect ${redirect}${safegivenname}?h=${rhandle}

    if {[info exists env(SERVER_PROTOCOL)] && [regexp {[Hh][Tt][Tt][PP]/([0-9]+)\.([0-9]+)} $env(SERVER_PROTOCOL) m vmaj vmin] && $vmaj >= 1 && $vmin >= 1} {
      cgi_puts "Status: 303 Temporary Redirect"
    } else {
      cgi_puts "Status: 302 Redirected"
    }

    cgi_puts "URI: ${redirect}${safegivenname}?h=${rhandle}${straytmp}"
    cgi_puts "Location: ${redirect}${safegivenname}?h=${rhandle}${straytmp}"
  }
}

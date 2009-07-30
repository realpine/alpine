#!./tclsh
# $Id: export.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  export.tcl
#
#  Purpose:  CGI script to download exported folder
#
#  Input:
set export_vars {
  {cid		"Unknown Command ID"}
  {fid		"Unknown Collection ID"}
}

#set export_via_ip_address 1
#set export_via_local_hostname 1

# inherit global config
source ./alpine.tcl

set mailextension ".mbx"

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


WPEval $export_vars {
  # generate filenames to hold exported folder and control file
  for {set n 0} {1} {incr n} {

    set rhandle [WPCmd PESession random 64]
    set cfile [file join $_wp(detachpath) detach.${rhandle}.control]
    set dfile [file join $_wp(detachpath) detach.${rhandle}.data]

    if {[file exists $cfile] == 0 && [file exists $dfile] == 0} {
      if {[catch {open $cfile {RDWR CREAT EXCL} [cgi_tmpfile_permissions]} cfd]
      || [catch {open $dfile {RDWR CREAT EXCL} [cgi_tmpfile_permissions]} dfd]} {
	if {[info exists dfd]} {
	  catch {close $cfd}
	  catch {file delete -force $cfile}
	  set errstr $dfd
	} else {
	  set errstr $cfd
	}

	error [list _action Export "Cannot create command/control files: [cgi_quote_html $errstr]" "Please close this window"]
      } else {
	close $dfd
	break
      }
    } elseif {$n > 4} {
      error [list _action Export "Command file creation limit" "Please close this window"]
    }
  }

  set colid [lindex $fid 0]
  set fldr [eval "file join [lrange $fid 1 end]"]

  catch {file delete $dfile}

  if {[catch {WPCmd PEFolder export $colid $fldr $dfile} result]} {
    WPCmd PEInfo statmsg $result
  } else {
    if {[set dfilesize [file size $dfile]] > 0
	&& ([info exists _wp(uplim_bytes)] && $_wp(uplim_bytes) > 0)
	&& $dfilesize > $_wp(uplim_bytes)} {
      if {$_wp(uplim_bytes) > (1000000)} {
	set dfs [format {%s.%.2s MB} [WPcomma [expr {$dfilesize / 1000000}]] [expr {$dfilesize % 1000000}]]
	set esl [format {%s.%.2s MB} [WPcomma [expr {$_wp(uplim_bytes) / 1000000}]] [expr {$_wp(uplim_bytes) % 1000000}]]
      } else {
	set dfs "[WPcomma $dfs] KB"
	set esl "[WPcomma $_wp(uplim_bytes)] KB"
      }

      WPCmd PEInfo statmsg "Exported folder size ($dfs) exceeds the maximum ($esl) size that can be imported.<br>If you wish to import this folder back into Web Alpine at a later time,<br>you should break it up into smaller folders"
    }

    if {[info exists export_via_ip_address]} {
      if {[regsub {^(http[s]?://)[A-Za-z0-9\\-\\.]+(.*)$} "[cgi_root]/pub/getach.tcl" "\\1\[[WPServerIP]\]\\2" redirect] != 1} {
	WPCmd PEInfo statmsg "Cannot determine server address"
	catch {unset redirect}
      }
    } elseif {[info exists export_via_local_hostname]} {
      if {[regsub {^(http[s]?://)[A-Za-z0-9\\-\\.]+(.*)$} "[cgi_root]/pub/getach.tcl" "\\1\[[info hostname]\]\\2" redirect] != 1} {
	WPCmd PEInfo statmsg "Cannot determine server address"
	catch {unset redirect}
      }
    } else {
      set redirect "[cgi_root]/pub/getach.tcl"
    }

    set givenname "[file tail $fldr]${mailextension}"
    set safegivenname $givenname
    regsub -all {[/]} $safegivenname {-} safegivenname
    regsub -all {[ ]} $safegivenname {_} safegivenname
    regsub -all {[\?]} $safegivenname {X} safegivenname
    regsub -all {[&]} $safegivenname {X} safegivenname
    regsub -all {[#]} $safegivenname {X} safegivenname
    regsub -all {[=]} $safegivenname {X} safegivenname
    set safegivenname "/$safegivenname"

    puts $cfd "Content-type: Application/X-Mail-Folder"
    puts $cfd "Content-Disposition: attachment; filename=\"$givenname\""

    # side-step the cgi_xxx stuff in this special case because
    # we don't want to buffer up the downloading attachment...

    puts $cfd "Content-Length: $dfilesize"
    puts $cfd "Expires: [clock format [expr {[clock seconds] + 3600}] -f {%a, %d %b %Y %H:%M:%S GMT} -gmt true]"
    puts $cfd "Cache-Control: max-age=3600"
    puts $cfd "Refresh: 0; URL=\"$_wp(serverpath)/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders\""
    puts $cfd ""

    puts $cfd $dfile

    # exec chmod [cgi_tmpfile_permissions] $dfile

    close $cfd

    exec /bin/chmod [cgi_tmpfile_permissions] $cfile
    exec /bin/chmod [cgi_tmpfile_permissions] $dfile
  }

  # prepare to clean up if the brower never redirects
  if {[info exists redirect]} {
    set redirect "${redirect}${safegivenname}?h=${rhandle}"
  } else {
    set redirect "wp.tcl?page=folders&cid=$cid"
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

  exec echo $rhandle | [file join $_wp(cgipath) [WPCmd PEInfo set wp_ver_dir] whackatch.tcl] >& /dev/null &
}

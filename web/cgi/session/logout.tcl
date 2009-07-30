#!./tclsh
# $Id: logout.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# Imported args
set logout_vars {
  {serverid	{}	0}
  {expinbox	{}	0}
  {expcurrent	{}	0}
  {cid		"Missing Command ID"}
}

# and any global config
source ./alpine.tcl

WPEval $logout_vars {
  
  if {$cid != [WPCmd PEInfo key]} {
    error [list _action Logout "Invalid Operation ID" "Please close this window."]
  }

  if {$expinbox &&  [catch {WPCmd PEMailbox expunge inbox} result]} {
    set logouterr $result
  }

  if {$expcurrent &&  [catch {WPCmd PEMailbox expunge current} result]} {
    if {[info exists logouterr] == 0} {
      set logouterr $result
    }
  }

  if {[catch {WPCmd PESession close} result]} {
    if {[info exists logouterr] == 0} {
      set logouterr $result
    }
  }

  if {[catch {WPCmd set wp_ver_dir} verdir]} {
    set verdir $_wp(appdir)
  }

  catch {WPCmd exit}

  cgi_http_head {
    set parms "?verdir=${verdir}"

    if {[regexp {^[0-9]+$} $serverid]} {
      append parms "&serverid=$serverid"
    }

    if {[info exists logouterr]} {
      append parms "&logerr=[WPPercentQuote $logouterr]"
    }

    cgi_redirect $_wp(serverpath)/$_wp(appdir)/farewell.tcl${parms}
  }
}


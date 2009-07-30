#!./tclsh
# $Id: logon.tcl 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $
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

#  logon.tcl
#
#  Purpose: CGI script to authenticate user based on provided
#           credentials and launch them into the mailbox index

# and any global config
source ./alpine.tcl

# don't use WPEval since it'll mask open's credential failure case
cgi_eval {

  if {$_wp(debug)} {
    cgi_debug -on
  }

  #
  # Import username and password from pubcookie, if possible.
  # Otherwise get it from the form that was submitted.
  #
  cgi_input

  if {[catch {
	       cgi_import sessid
	       WPValidId $sessid
	     } result]} {
    WPInfoPage "Web Alpine Error" [font size=+2 "$result"] \
	       "Please complain to the [cgi_link Admin] and visit the [cgi_link Start] later."
    return
  }

  if {[catch {WPCmd set wp_open_parms} parms]} {
    WPInfoPage "Internal Error" [font size=+2 $parms] \
	       "Please complain to the [link Admin] and visit the [cgi_link Start] later."
  } else {
    catch {WPCmd unset wp_open_parms}

    foreach {p v} $parms {
      set $p $v
    }

    if {[catch {WPCmd PESession open $User $confloc $defconf} answer]} {
      if {0 == [string length $answer] || 0 == [string compare BADPASSWD [lindex $answer 0]]} {
	set answer "Unknown Username or Incorrect Password"
      }

      set alerts {}
      if {[catch {WPCmd PEInfo statmsgs} statmsgs] == 0} {
	# display any IMAP alerts
	foreach m $statmsgs {
	  if {[regexp {^Alert received.*\[ALERT\] (.*)$} $m dummy a]} {
	    if {[lsearch -exact $alerts $a] < 0} {
	      lappend alerts $a
	    }
	  }
	}
      }

      WPInfoPage "Login Failure" [font size=+2 $answer] \
	  "Please click your browser's [bold Back] button to return to the [cgi_link Start] to try again..." \
	  {} [join $alerts "<br>"]

      # unlaunch the thing
      catch {WPCmd PESession close}
      catch {WPCmd exit}
      return
    }

    # determine suitable number of index lines for the indicated display size
    # based on:
    #
    #  1. a header length of 72 pixels
    #  2. a TD font-size plus padding of 24 points
    #

    set indexheight [WPCmd PEInfo indexheight]
    if {[string length $indexheight] == 0} { set indexheight $_wp(indexheight)}
    if {[info exists hPx] && [regexp {^[0-9]+$} $hPx]} {
      # "66" comes from _wp(titlethick) + _wp(titlesep) + ((index tables cellpaddings * 2) = 8) + some fudge
      set indexlines [expr (($hPx - 66) / $indexheight) - 1]
    }

    if {[info exists indexlines] == 0 || $indexlines <= 0} {
      set indexlines [WPCmd PEInfo indexlines]
    }

    if {$indexlines <= 0} {
      set indexlines $_wp(indexlines)
    }

    # start with the message indicated by the
    # 'incoming-startup-rule' in the current index
    set firstmsg 1
    if {![catch {WPCmd PEMailbox firstinteresting} firstint] && $firstint > 0} {
      set messagecount [WPCmd PEMailbox messagecount]
      for {set i 1} {$i < $messagecount} {incr i $indexlines} {
	if {$i >= $firstint} {
	  break
	}

	set firstmsg $i
      }

      # show whole last page
      if {$firstmsg + $indexlines > $messagecount} {
	if {[set n [expr ($messagecount + 1) - $indexlines]] > 0} {
	  set firstmsg $n
	} else {
	  set firstmsg 1
	}
      }
    }

    if {[catch {WPCmd PEInfo sort} defsort]} {
      set defsort {Date 0}
    }

    # set these in alpined's interp so they're fished out by WPImport
    if {[catch {
      WPCmd set sort [lindex $defsort 0]
      WPCmd set rev [lindex $defsort 1]
      WPCmd set ppg $indexlines
      WPCmd set width $_wp(width)
      WPCmd set serverid $Server} result]} {
      WPInfoPage "Initialization Failure" [font size=+2 $result] \
	  "Please click your browser's [bold Back] button to return to the [cgi_link Start] to try again..."
      catch {WPCmd PESession close}
      catch {WPCmd exit}
      return
    }

    if {[catch {WPCmd PEMailbox uid $firstmsg} exp]} {
      set exp 1
    }

    WPCmd set top $exp

    if {[catch {WPCmd set serverroot} serverroot] == 0} {
      cgi_root $serverroot
    }

    set startpage "[cgi_root]/${startpage}?sessid=$sessid"

    if {[string length $prunepage] && [WPCmd PEInfo prunecheck] == 1} {
      set startpage "[cgi_root]/${prunepage}cid=[WPCmd PEInfo key]&sessid=${sessid}&start=[WPPercentQuote ${startpage}]"
    }

    cgi_http_head {
      if {[info exists env(REMOTE_USER)]} {
	# redirect thru intermediate so session id and secured user name can get bound in uidampper
	cgi_redirect $_wp(serverpath)/session/startup.tcl?sessid=${sessid}&page=[WPPercentQuote $startpage]
      } else {
	cgi_redirect $startpage
      }
    }
  }
}

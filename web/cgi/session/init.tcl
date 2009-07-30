#!./tclsh
# $Id: init.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  init.tcl
#
#  Purpose: CGI script to establish foundation for webpine session

# and any global config
source ./alpine.tcl


cgi_eval {
  if {$_wp(debug)} {
    cgi_debug -on
  }

  #
  # Import username and password from pubcookie, if possible.
  # Otherwise get it from the form that was submitted.
  #
  cgi_input

  if {[catch {cgi_import User}] || 0 == [string length $User]} {
    WPInfoPage "Bogus Username" \
	"[font size=+2 "Sorry, didn't catch your [bold name]!"]" \
	"Please click your browser's [bold Back] button to return to the [cgi_link Start], and fill in a [italic Username]..."
    return
  }
  
  if {[catch {cgi_import Pass}]} {
    set Pass ""
  }

  if {[catch {cgi_import Server}] || 0 == [string length $Server]} {
    WPInfoPage "Bogus Server" \
	"[font size=+2 "Invalid Server specified"]" \
	"Please click your browser's [bold Back] button to return to the [cgi_link Start], and fill in a [italic Server]..."
    return
  }

  catch {cgi_import hPx}

  set defconf [file join $_wp(confdir) $_wp(defconf)]
  set confloc ""

  if {[string length $Server] < 256 && 0 == [regexp {[[:cntrl:]]} $Server]} {
    if {[info exists _wp(hosts)] && $Server >= 0 && $Server < [llength $_wp(hosts)]} {
      set sdata [lindex $_wp(hosts) $Server]

      set env(IMAP_SERVER) "[subst [lindex $sdata 1]]/user=$User"

      if {[llength $sdata] > 2 && [string length [lindex $sdata 2]]} {
	set defconf [subst [lindex $sdata 2]]
      } else {
	#
	# Validate input?
	#
	WPInfoPage "Internal Error" \
	    [font size=+2 "IMAP Server Mismatch"] \
	    "Please complain to the [link Admin] and visit the [cgi_link Start] later."
	return
      }
    } elseif {[regexp {/user=} $Server]} {
      set env(IMAP_SERVER) "$Server"
    } else {
      set env(IMAP_SERVER) "$Server/user=$User"
    }

    set confloc "\{$env(IMAP_SERVER)\}$_wp(config)"

    regexp {^[^:/]*} $env(IMAP_SERVER) env(IMAP_SERVER_BASE)
  } else {
    WPInfoPage "Bad Server Name" [font size=+2 "Server Name too long or has bogus characters."] \
	"Please click your browser's [bold Back] button to return to the [cgi_link Start] to try again..."
    return
  }

  set confloc "\{$env(IMAP_SERVER)\}$_wp(config)"

  if {[catch {regexp {^[^:/]*} $env(IMAP_SERVER) env(IMAP_SERVER_BASE)}]} {
    set env(IMAP_SERVER_BASE) ""
  }

  # in less rigid settings, it might make sense to allow
  # for random input folder names...
  # cgi_import Folder

  #
  # Server, folder and credentials in hand, fork the client...
  # <OL>
  #    <LI> The session is *assumed* to run over SSL.
  #    <LI> The server is *assumed* to be a black box
  #         (no, possibly hostile, user shells)
  #    <LI> We need to run the alpine process as the given user.
  #         Unless we bind to a specific server, http authentication
  #         isn't sufficient as t
  #       
  #	 <LI> The session-id connects future requests to the newly
  #         created alpine engine.
  #    <LI> The auth-cookie will tell us the session-id isn't coming from
  #         j. random cracker's client
  # </OL>
  #

  if {[catch {exec [file join $_wp(bin) launch.tcl]} _wp(sessid)]} {
    WPInfoPage "Internal Error" [font size=+2 $_wp(sessid)] \
	"Please complain to the [link Admin] and visit the [cgi_link Start] later."
    return
  } else {
    WPValidId $_wp(sessid)
  }

  if {[catch {cgi_import ssl}] || $ssl == 0} {
    WPCmd set serverroot $_wp(plainservpath)
    cgi_root $_wp(plainservpath)
  }

  # stash login credentials away for later
  if {[catch {
		WPCmd set nojs 1
		WPCmd PESession creds 0 $confloc $User $Pass
	     } result]} {
    WPInfoPage "Initialization Failure" [font size=+2 "Initialization Failure: $result"] \
	"Please click your browser's [bold Back] button to return to the [cgi_link Start] to try again..."
    catch {WPCmd exit}
    return
  }

  set cookiepath $_wp(appdir)

  # stash session open parms in alpined's interpreter
  lappend parms User
  lappend parms $User
  lappend parms Server
  lappend parms $Server
  lappend parms confloc
  lappend parms $confloc
  lappend parms defconf
  lappend parms $defconf
  lappend parms startpage

  lappend parms "$_wp(appdir)/$_wp(ui2dir)/browse/0/INBOX"
  lappend parms prunepage
  lappend parms ""

  if {[info exists hPx]} {
    lappend parms hPx
    lappend parms $hPx
  }

  if {[catch {WPCmd set wp_open_parms $parms} result]} {
    WPInfoPage "Internal Error" [font size=+2 $result] \
	"Please complain to the [link Admin] and visit the [cgi_link Start] later."
    return
  }

  # return a page that says we're logging in the user
  # have that page return to opening the session...

  catch {WPCmd set wp_ver_dir $cookiepath}

  set sessid "$_wp(sessid)@[info hostname]"

  cgi_http_head {
    WPExportCookie sessid $sessid $cookiepath
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      cgi_http_equiv Refresh "0; url=$_wp(serverpath)/session/logon.tcl?sessid=$sessid"
    }

    cgi_body {
      cgi_table height="20%" {
	cgi_table_row {
	  cgi_table_data {
	    cgi_puts [cgi_nbspace]
	  }
	}
      }

      cgi_center {
	cgi_table border=0 width=500 cellpadding=3 {
	  cgi_table_row {
	    cgi_table_data align=center rowspan=2 {
	      cgi_put [cgi_imglink logo]
	    }

	    cgi_table_data rowspan=2 {
	      cgi_put [cgi_img [WPimg dot2] border=0 width=18]
	    }

	    cgi_table_data {
	      cgi_puts [cgi_font size=+2 "Logging into $_wp(appname)"]
	    }
	  }

	  cgi_table_row {
	    cgi_table_data {
	      cgi_puts "Please be patient!  Depending on Inbox size, server load and other factors this may take a moment [cgi_img [WPimg dotblink]]"
	    }
	  }
	}
      }
    }
  }
}

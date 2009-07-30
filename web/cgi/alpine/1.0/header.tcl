#!./tclsh
# $Id: header.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  header.tcl
#
#  Purpose:  CGI script to generate generic header for 
#	     webpine-lite pages.  the idea is that this
#            page goes in the {title,nav}-bar portion of a
#            framed page so we keep the servlet alive via
#            periodic reloads while more static/form stuff
#            is displayed in the "body" frame.

#  Input:
set header_vars {
  {title	""	"0,0"}
  {reload}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set nologo 0
set about {}

WPEval $header_vars {

  set tv [split $title ","]
  switch -- [lindex $tv 0] {
    10 {
      set title_text "Compose New Message"
    }
    20 {
      set title_text "Postponed Message"
    }
    30 {
      set title_text "Forward Message"
      if {[regexp {^[0-9]*$} [lindex $tv 1]]} {
	append title_text " [lindex $tv 1]"
      }
    }
    40 {
      set title_text "Reply to Message"
      if {[regexp {^[0-9]*$} [lindex $tv 1]]} {
	append title_text " [lindex $tv 1]"
      }
    }
    50 {
      set title_text "Message Index"
      set nologo 1
    }
    60 {
      set title_text "Help System"
    }
    70 {
      set title_text "Address Books"
    }
    71 {
      set title_text "Edit Address Book Entry"
    }
    72 {
      set title_text "New Address Book Entry"
    }
    73 {
      set title_text "Address Selection"
    }
    74 {
      set title_text "LDAP Query Result Selection"
    }
    80 {
      set title_text "Select Postponed Message"
    }
    90 {
      set title_text "Expunge Deleted Messages"
      set nologo 1
    }
    101 {
      set title_text "Select Messages by Date"
      set nologo 1
    }
    102 {
      set title_text "Select Messages by Text"
      set nologo 1
    }
    103 {
      set title_text "Select Messages by Status"
      set nologo 1
    }
    104 {
      set title_text "Search and Mark Messages"
      set nologo 1
    }
    110 {
      set title_text "Attach a File"
    }
    120 {
      set title_text "File Creation Confirmation"
      set nologo 1
    }
    130 {
      set title_text "Authentication Required"
    }
    140 {
      set title_text "Web Alpine Help"
    }
    150 {
      set title_text "Configuration"
    }
    151 {
      set title_text "Add Collection Configuration"
    }
    152 {
      set title_text "Edit Collection Configuration"
    }
    153 {
      set title_text "Add Filter Configuration"
    }
    154 {
      set title_text "Edit Filter Configuration"
    }
    160 {
      set title_text "Set Message Flags"
      set nologo 1
    }
    170 {
      set title_text "Confirm Folder Delete"
    }
    171 {
      set title_text "New Folder Creation"
    }
    172 {
      set title_text "New Directory Creation"
    }
    173 {
      set title_text "Folder Rename"
    }
    174 {
      set title_text "Create New Folder or Directory"
    }
    180 {
      set title_text "Spell Check Composition"
    }
    190 {
      set title_text "Save Messages"
      set nologo 1
    }
    200 {
      set title_text "Attachment Display"
    }
    210 {
      set title_text "Take Addresses"
    }
    211 {
      set title_text "Take Address Edit"
    }
    212 {
      set title_text "Take Address Same Nickname"
    }
    220 {
      set title_text "Folder List for Save"
      set nologo 1
    }
    221 {
      set title_text "Folder List for Save"
    }
    222 {
      set title_text "Folder To Save To"
      set nologo 1
    }
    223 {
      set title_text "Folder To Save To"
    }
    230 {
      set title_text "Quitting Web Alpine"
    }
    240 {
      set title_text "LDAP Query"
    }
    250 {
      set title_text "Folder Upload and Import"
    }
    260 {
      set title_text "Monthly Folder Clean Up"
    }
    default {
      if {[catch {WPCmd PEInfo set wp_header_title} title_text]} {
	set title_text Untitled
      }
    }
  }

  WPCmd PEInfo set wp_header_title $title_text

  if {[catch {WPNewMail $reload} newmail]} {
    error [list _action "new mail" $newmail]
  }

  cgi_http_head {
    WPStdHttpHdrs text/html
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr Header
      WPHtmlHdrReload "$_wp(appdir)/$_wp(ui1dir)/header.tcl?sessid=$sessid"
      WPStyleSheets
    }

    cgi_body bgcolor=$_wp(bordercolor) background=[file join $_wp(imagepath) logo $_wp(logodir) back.gif] "style=\"background-repeat: repeat-x\"" {
      WPTFTitle $title_text $newmail $nologo $about
    }
  }
}

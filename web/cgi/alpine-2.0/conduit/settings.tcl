#!./tclsh
# $Id: settings.tcl 1164 2008-08-22 19:46:13Z mikes@u.washington.edu $
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

#  settings.tcl
#
#  Purpose:  CGI script to do the job of updating submitted settings
# 

#  Input: 
set settings_vars {
  {restore	""	false}
}

# inherit global config
source ../alpine.tcl
source ../common.tcl

#  Output: HTML containing javascript calls to functions of parent window
# 


# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

foreach item $settings_vars {
  if {[catch {eval WPImport $item} result]} {
    regsub -all {"} $result {\"} errstr
    break
  }
}

set response "Settings Updated"

# feature settings
if {0 == [string compare restore $restore]} {
  WPCmd PEConfig reset pinerc
  set response "Default Settings Restored"
} elseif {[catch {
  WPCmd PEConfig newconf

  # collect feature settings
  set setfeatures [WPCmd PEConfig featuresettings]
  foreach feature {
    include-header-in-reply
    include-attachments-in-reply
    signature-at-bottom
    strip-from-sigdashes-on-reply
    fcc-without-attachments
    auto-move-read-msgs
    enable-msg-view-urls
    feature enable-msg-view-web-hostnames
    enable-msg-view-addresses
    render-html-internally
    quell-server-after-link-in-html
    quell-flowed-text
  } {
    if {[catch {cgi_import_as $feature setting}]} {
      if {[lsearch $setfeatures $feature] >= 0} {
	WPCmd PEConfig feature $feature 0
      }
    } else {
      if {[lsearch $setfeatures $feature] < 0} {
	WPCmd PEConfig feature $feature 1
      }
    }
  }

  # collect single-line text variable settings
  foreach variable {
    wp-indexlines
    default-fcc
    personal-name
    sort-key
    posting-character-set
    reply-leadin
    reply-indent-string
    postponed-folder
    trash-folder
    inbox-path
    rss-news
    rss-weather
  } {
    set varvals [WPCmd PEConfig varget $variable]
    set varvalue [lindex $varvals 0]
    cgi_import_as $variable value
    set value [split $value "\n"]
    if {[string compare $varvalue $value]} {
      WPCmd PEConfig varset $variable $value
    }
  }

  # collect list-style variable settings
  foreach {variable handle} {
    alt-addresses		altAddr
    viewer-hdrs			viewerHdr
    default-composer-hdrs	composeHdr
    smtp-server			smtpServer
    ldap-servers		ldapServer
  } {
    if {0 == [catch {cgi_import_as ${handle}s lcount}]} {
      set l {}
      for {set i 1} {$i <=  $lcount} {incr i} {
	if {0 == [catch {cgi_import_as ${handle}${i} value}] && [string length $value]} {
	  lappend l $value
	}
      }

      WPCmd PEConfig varset $variable $l
    }
  }

  # special-case variable settings
  if {0 == [catch {cgi_import customHdrFields}]} {
    set hdrs {}
    for {set i 1} {$i <=  $customHdrFields} {incr i} {
      if {0 == [catch {cgi_import_as customHdrField${i} field}]} {
	set hdr "${field}:"
	if {0 == [catch {cgi_import_as customHdrData${i} value}]} {
	  append hdr " $value"
	}

	lappend hdrs $hdr
      }
    }

    if {[llength $hdrs]} {
      WPCmd PEConfig varset customized-hdrs $hdrs
    }
  }

  cgi_import wrapColumn
  WPCmd PEConfig columns $wrapColumn

  cgi_import folderCache
  WPSessionState left_column_folders $folderCache

  cgi_import forwardAs
  if {0 == [string compare attached $forwardAs]} {
    if {[lsearch $setfeatures $feature] < 0} {
      WPCmd PEConfig feature forward-as-attachment 1
    }
  } else {
    if {[lsearch $setfeatures $feature] >= 0} {
      WPCmd PEConfig feature forward-as-attachment 0
    }
  }

  cgi_import signature
  set cursig [string trimright [join [WPCmd PEConfig rawsig] "\n"]]
  set signature [string trimright $signature]
  if {[string compare $cursig $signature]} {
    WPCmd PEConfig rawsig [split $signature "\n"]
  }

  WPCmd PEConfig saveconf
} result]} {
  regsub -all {"} $result {\"} errstr
}

puts stdout "Content-type: text/html;\n\n<html><head><script>"

if {[info exists errstr]} {
  puts stdout "window.parent.settingsFailure(\"${errstr}\");"
} else {
  WPCmd PEInfo statmsg $response
  puts stdout "window.parent.settingsSuccess();"
}

puts stdout "</script></head><body></body></html>"

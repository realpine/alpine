#!./tclsh

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

#  getach.tcl
#
#  Purpose:  CGI script to retrieve requested attachment
#
#  Input:
#
#    h : handle referring to temp file holding attachment particulars

# inherit global config
source ./alpine.tcl

cgi_eval {

  # verify that the format of the request is correct.  Mainly it's
  # to reasonably handle relative urls in viewed html attachments.  Ugh.
  if {[regexp {^h=[A-Za-z0-9]*$} $env(QUERY_STRING)]} {

    cgi_input

    WPImport h

    if {[regexp {^[A-Za-z0-9]+$} $h] != 1} {
      WPInfoPage "Web Alpine Error" [font size=+2 "Bogus Attachment Handle: $h"] "Please close this window."
      exit
    }

    set cmdfile [file join $_wp(detachpath) detach.${h}-control]

    if {[file exists $cmdfile] == 0} {
      WPInfoPage "Webpine Error" [font size=+2 "Stale Handle Reference"] "Please click the attachment link to view this attachment."
    } elseif {[catch {
      set cid [open $cmdfile r]

      while {1} {
	if {[gets $cid line] == 0} {
	  if {[gets $cid tmpfile] != 0} {
	    puts stdout ""

	    set fp [open $tmpfile r]

	    fconfigure $fp -translation binary
	    fconfigure stdout -translation binary

	    fcopy $fp stdout

	    close $fp 
	  }

	  break
	} else {
	  puts stdout $line
	}
      }
    } errstr]} {
      WPInfoPage "Webpine Error" [font size=+2 $errstr] "Please close this window."
    }

    #
    # Don't delete the temp files immediately because *some* browsers
    # <cough>IE<cough> will download the attachment, then hand the
    # URL of the downloaded attachment to the viewer <cough>WMP<cough>
    # so the viewer can then *REFETCH* the data for display.  No,
    # they don't pay attention to the redirect code.
    #
    # The script that created them is responsible for having set in motion
    # a process to delete them at some point in the future.
    #
    # catch {file delete -force $cmdfile}
    # catch {file delete -force $tmpfile}
  } else {
    WPInfoPage "Invalid Attachment Reference" "[font size=+2 "Invalid Attachment Reference"]" \
	"This page is the result of clicking a link in an attached HTML document.  It's author likely used an incomplete hypertext link which resulted in a bogus link to the WebPine server.<p>Clicking your browser's Back button should restore the attachment's display."
  }
}

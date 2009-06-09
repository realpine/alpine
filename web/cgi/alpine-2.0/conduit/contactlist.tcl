#!./tclsh
# $Id: contactlist.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  contactlist.tcl
#
#  Purpose:  CGI script that generates a page displaying a 
#            list of contacts in the requested address book
#
#  Input:    PATH_INFO: /booknumber
#            along with possible search parameters:
set contactlist_args {
  {op		{}	"noop"}
  {book		""	0}
  {ai		""	-1}
  {entryList	""	""}
  {contactNick	""	""}
  {origNick	""	""}
  {contactName	""	""}
  {contactEmail	""	""}
  {contactFcc	""	""}
  {contactNotes	""	""}
  {hdr		{}	"off"}
  {sendto	{}	"off"}
  {canedit	{}	"off"}
}

# inherit global config
source ../alpine.tcl
source ../common.tcl

# TEST
proc cgi_suffix {args} {
  return ""
}

proc deleteByBook {_delbooks} {
  upvar 1 $_delbooks delbooks
  foreach dbn [array names delbooks] {
    foreach dbi [lsort -integer -decreasing $delbooks($dbn)] {
      if {[catch {WPCmd PEAddress delete $dbn "" $dbi} result]} {
	error "Address Delete Failure: $result"
      }
    }
  }
}

if {[info exists env(PATH_INFO)] && [string length $env(PATH_INFO)]} {
  if {[regexp {^/([0-9]+)$} $env(PATH_INFO) dummy abook]} {
    # Import data validate it and get session id
    if {[catch {WPGetInputAndID sessid}]} {
      set harderr "No Session ID: $sessid"
    } else {
      # grok parameters
      foreach item $contactlist_args {
	if {[catch {eval WPImport $item} importerr]} {
	  set harderr "Cannot init session: $importerr"
	  break
	}
      }
    }
  } else {
    set harderr "Bad Address Book Request: $env(PATH_INFO)"
  }
} else {
  set harderr "No Address Book Specified"
}

puts stdout "Content-type: text/html; charset=\"UTF-8\"\n"

if {[info exists harderr]} {
  puts stdout "<b>ERROR: $harderr</b>"
  exit
}

switch -- $op {
  delete {
    set result ""
    # grok delete format: \[(book)index\],
    if {[regexp {^[0-9\.,]+$} $entryList]} {
      # collect by book
      set entryList [split $entryList ","]
      foreach e $entryList {
	if {[regexp {^([0-9]+)\.([0-9]+)} $e dummy eb ei]} {
	  lappend delbooks($eb) $ei
	}
      }

      # delete by book in DECREASING index order
      if {[catch {deleteByBook delbooks} result] || [string length $result]} {
	WPCmd PEInfo statmsg "$result"
      } else {
	if {[llength $entryList] > 1} {
	  WPCmd PEInfo statmsg "Contacts Deleted"
	} else {
	  WPCmd PEInfo statmsg "Contact Deleted"
	}
      }
    } else {
      WPCmd PEInfo statmsg "Invalid entry list format: >>>>$entryList<<<<"
    }
  }
  add {
    if {[catch {WPCmd PEAddress edit $book $contactNick $ai $contactName $contactEmail $contactFcc $contactNotes 1} result]} {
      WPCmd PEInfo statmsg "Add failed: $result"
    }
  }
  change {
    if {[catch {WPCmd PEAddress edit $book $contactNick $ai $contactName $contactEmail $contactFcc $contactNotes 0 $origNick} result]} {
      WPCmd PEInfo statmsg "Change failed: $result"
    }
  }
  noop {
  }
  default {
    WPCmd PEInfo statmsg "Unrecognized option: $op"
  }
}

# remainder is director to list
if {[catch {WPCmd PEAddress books} booklist]} {
  WPCmd PEInfo statmsg "Cannot get list of Addressbooks"
} else {
  set books [llength $booklist]
  for {set i 0} {$i < $booklist} {incr i} {
    if {$i == $abook} {
      set thisbook [lindex $booklist $i]
      if {[catch {WPCmd PEAddress list [lindex $thisbook 0]} clist]} {
	WPCmd PEInfo statmsg "Cannot get Contacts list: $clist"
      }

      if {[catch {WPCmd PEAddress format [lindex $thisbook 0]} format]} {
	WPCmd PEInfo statmsg "Cannot get Contacts format: $format"
      }

      break
    }
  }

  if {[info exists thisbook]} {
    cgi_division class="clistContext" {
      if {$books == 1} {
	cgi_put "[cgi_span "class=sp spfcl spfcl1" [cgi_span "style=display:none;" "Folders: "]][cgi_span "Contact List"]"
      } else {
	cgi_put "[cgi_span "class=sp spfcl spfcl1" "style=border-bottom: 1px solid #003399;" [cgi_span "style=display:none;" "Contacts: "]]"
	cgi_put [cgi_span id=clistName [lindex $thisbook 1]]
      }
    }

    cgi_division class=clistContacts id=clistContacts {
      cgi_table cellspacing="0" cellpadding="0" "class=\"listTbl divider clt\"" {
	cgi_puts "<tbody>"
	if {![string compare $hdr on]} {
	  cgi_table_row "class=\"contactHeader\"" {
	    cgi_table_head "class=\"wap colHdr\"" {}
	    foreach field $format {
	      cgi_table_head "class=\"wap colHdr lt\"" {
		switch -- [lindex $field 0] {
		  nick {
		    cgi_puts "Nickname"
		  }
		  full {
		    cgi_puts "Display Name"
		    if {0 == [string compare $canedit on]} {
		      cgi_puts " (click to edit)"
		    }
		  }
		  addr {
		    cgi_put "Email"
		    if {0 == [string compare $sendto on]} {
		      cgi_puts " (click email to send)"
		    }
		  }
		  default {
		    cgi_puts [index $field 0]
		  }
		}
	      }
	    }
	  }
	}

	set aindex 0
	foreach contact $clist {
	  cgi_table_row "class=\"clr\"" {
	    set nfields [llength $format]

	    cgi_table_data class=wap {
	      set label "ab${abook}.${aindex}"
	      cgi_checkbox "nickList=$abook.$aindex.[lindex [lindex $contact 0] 0]" id=$label "onclick=\"return boxChecked(this);\""
	    }

	    for {set i 0} {$i < $nfields} {incr i} {
	      set field [lindex $format $i]
	      set data [lindex $contact [expr $i + 1]]

	      cgi_table_data "class=\"wap clcd\"" {
		switch -- [lindex $field 0] {
		  addr {
		    switch -- [lindex [lindex $contact 0] 1] {
		      single {
			set addr [cgi_quote_html $data]
			if {0 == [string compare $sendto on]} {
			  cgi_puts [cgi_url $addr compose?contacts=${abook}.${aindex} class=wap]
			} else {
			  cgi_puts "<label for=$label>$addr</label>"
			}
		      }
		      list {
			foreach addr $data {
			  set addr [cgi_quote_html $addr]
			  if {0 == [string compare $sendto on]} {
			    cgi_puts "[cgi_url $addr compose?contacts=${abook}.${aindex} class=wap][cgi_nl]"
			  }
			}
		      }
		      default {
			cgi_puts "Unknown contact type"
		      }
		    }
		  }
		  full {
		    if {[string length $data]} {
		      if {0 == [string compare $canedit on]} {
			cgi_puts [cgi_url [cgi_quote_html $data] # "onClick=return editContact({book:${abook},index:${aindex}});" class=wap]
		      } else {
			cgi_puts "<label for=$label>[cgi_quote_html $data]</label>"
		      }
		    } else {
		      cgi_puts [cgi_nbspace]
		    }
		  }
		  default {
		    if {[string length $data]} {
		      cgi_puts "<label for=$label>[cgi_quote_html $data]</label>"
		    } else {
		      cgi_puts [cgi_nbspace]
		    }
		  }
		}
	      }
	    }
	  }
	  incr aindex
	}
	cgi_puts "</tbody>"
      }
    }
  } else {
    WPCmd PEInfo statmsg "Unknown Address Book"
  }
}
cgi_puts "<script>"
wpStatusAndNewmailJavascript
cgi_puts "if(window.updateContactCount) updateContactCount('$books','[llength $clist]');"
cgi_puts "</script>"

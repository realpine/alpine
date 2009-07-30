#!./tclsh
# $Id: addredit.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  addredit.tcl
#
#  Purpose:  CGI script to generate html form used to view/set
#	     individual addressbook entries
#
#  Input: 
set ae_vars {
    {book	{} -1}
    {nick       {} ""}
    {add        {} 0}
    {fn         {} ""}
    {addrs      {} ""}
    {fcc        {} ""}
    {comment    {} ""}
    {take       {} 0}
    {newnick    {} ""}
    {ai         {} -1}
}

set ae_fields {
    {0 "newnick" "Nickname<sup class=notice>*</sup>"}
    {1 "fn" "Full Name&nbsp;"}
    {2 "addrs" "Addresses<sup class=notice>*</sup>"}
    {3 "fcc" "Fcc&nbsp;"}
    {4 "comment" "Comments&nbsp;"}
}    

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

#  Output: 
#
#

set ae_menu {
  {
    {}
    {
      {
	cgi_image_button help=[WPimg help_trans] border=0 alt="Help"
      }
    }
  }
}

WPEval $ae_vars {

  if {$book < 0} {
    if {[catch {WPCmd PEInfo set ae_help_state} ae_help_state] == 0} {
      foreach v $ae_help_state {
	eval set [lindex $v 0] [list [lindex $v 1]]
      }

      set addrinfo [list "$newnick"  "$fn" [list "$addrs"] "$fcc" "$comment"]
      WPCmd PEInfo unset ae_help_state
    } else {
      return [list _action "Web Alpine" "Unspecified Address Book"]
    }
  }

  catch {WPCmd PEInfo unset ae_help_state}

  if {![info exists addrinfo]} {
    if {$take != 0} {
      set addrinfo [list "$newnick"  "$fn" [list "$addrs"] "$fcc" "$comment"]
    } elseif {$add == 0} {
      if {[catch {WPCmd PEAddress fullentry $book $nick $ai} addrinfo]} {
	if {[string length $addrinfo]} {
	  set entryerror "Address Error: $addrinfo"
	} else {
	  set entryerror "Nickname $nick does not exist"
	}

	set addrinfo [list "" "" [list ""] "" ""]
      }
    } else {
      set addrinfo [list "" "" [list ""] "" ""]
    }
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Addressbook Update"
      WPStyleSheets
    }

    if {$take == 1} {
      set onload "onLoad=document.addredit.newnick.focus()"
    } else {
      set onload ""
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" $onload {
      set books [WPCmd PEAddress books]
      set readwrite [lindex [lindex $books $book] 3]

      catch {WPCmd PEInfo set help_context addredit}
      cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {
	cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post enctype=multipart/form-data name=addredit target=_top {
	  cgi_table_row {
	    #
	    # next comes the menu down the left side
	    #
	    eval {
	      cgi_table_data $_wp(menuargs) rowspan=1000 {
		WPTFCommandMenu ae_menu {}
	      }
	    }

	    cgi_table_row {
	      cgi_table_data valign=top width="100%" class=dialog {
		if {[info exists entryerror]} {
		  cgi_division class=notice align=center {
		    cgi_puts $entryerror
		  }
		}


		if {$take == 1} {
		  lappend tmptxt "Edit the new entry below as neccessary (note, some are required).  To create a list entry, simply add each desired address to the [cgi_italic Addresses] field separated by a comma."
		  lappend tmptxt "When finished, click [cgi_italic Save] to update your address book, or [cgi_italic Cancel] to return to the message view."
		} elseif {$add == 1} {
		  lappend tmptxt "The address book entry editor is used to create a new address book entry.  Fill in the fields as desired below (note, some are required).  To create a list entry, simply add each desired address to the [cgi_italic Addresses] field separated by a comma."
		  lappend tmptxt "When finished, click [cgi_italic Save] to update your address book, or [cgi_italic Cancel] to return to your unchanged address book."
		} elseif {$readwrite == 0} {
		  set tmptxt "These are the current settings for the selected entry"
		} else {
		  lappend tmptxt "The address book entry editor is used to edit an existing address book entry.  Edit the fields as desired below (note, some are required)."
		  lappend tmptxt " then click [cgi_italic Save] to update your address book, or [cgi_italic Cancel] to return to your unchanged address book."
		}

		cgi_table align=center width=75% cellpadding=10 border=0 {
		  foreach t $tmptxt {
		    cgi_table_row {
		      cgi_table_data align=center {
			cgi_puts $t
		      }
		    }
		  }
		}

		cgi_table border=0 cellspacing=0 cellpadding=5 align=center {
		  cgi_text "page=addrsave" type=hidden notab

		  if {$take == 1} {
		    cgi_text "oncancel=main" type=hidden notab
		    cgi_text "take=1" type=hidden notab
		  } else {
		    cgi_text "oncancel=addrbook" type=hidden notab
		  }

		  cgi_text "book=$book" type=hidden notab
		  if {$add == 0} {cgi_text "nick=$nick" type=hidden notab}
		  if {$add != 0} {cgi_text "add=1" type=hidden notab}
		  cgi_text "ai=${ai}" type=hidden notab
		  cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab

		  foreach fieldval $ae_fields {
		    cgi_table_row {
		      cgi_table_data valign=top align=right width="30%" class=dialog {
			#						    cgi_puts [cgi_font face=tahoma,verdana,geneva size=+1 "[lindex $fieldval 2]:"]
			cgi_puts [cgi_bold "[lindex $fieldval 2]:"]
		      }
		      cgi_table_data align=left {
			switch -regexp [lindex $fieldval 1] {
			  ^addrs$ {
			    set addrvals [lindex $addrinfo [lindex $fieldval 0]]
			    set line [join $addrvals ", "]
			    cgi_text "[lindex $fieldval 1]=${line}" size=50
			  }
			  default {
			    cgi_text "[lindex $fieldval 1]=[lindex $addrinfo [lindex $fieldval 0]]" size=50
			  }
			}
		      }
		    }
		  }
		  cgi_table_row {
		    cgi_table_data align=right {
		      cgi_puts [cgi_font class=notice size=-1 "* Required field"]
		    }
		  }
		  cgi_table_row {
		    cgi_table_data align=center colspan=2 {
		      if {$readwrite} {
			cgi_submit_button "save=Save Entry"
			cgi_put [cgi_img [WPimg dot2] border=0 alt="" width=10]
		      }

		      if {$readwrite && $add == 0 && $take == 0} {
			cgi_submit_button "delete=Delete Entry"
			cgi_put [cgi_img [WPimg dot2] border=0 alt="" width=10]
		      }

		      cgi_submit_button "cancel=Cancel"
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

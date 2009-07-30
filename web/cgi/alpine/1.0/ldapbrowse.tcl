#!./tclsh
# $Id: ldapbrowse.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  ldapbrowse.tcl
#
#  Purpose:  CGI script to browse ldap results

#  Input: [expected to be set when we get here]
set browse_vars {
  {field	"Missing Field Name"}
  {ldapquery	"Missing LDAP Query Number"}
  {addresses	""	""}
  {oncancel	""	""}
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set ldapres_cmds {
  {
    {}
    {
      {
	# * * * * USE ADDRESSES * * * *
	cgi_submit_button "address=Address" class="navtext"
      }
    }
  }
  {
    {}
    {
      {
	# * * * * CANCEL * * * *
	cgi_submit_button "cancel=Cancel" class="navtext"
      }
    }
  }
}

WPEval $browse_vars {

  if {[catch {WPCmd PEInfo noop} result]} {
    error [list _action "No Op" $result]
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    if {$ldapquery != 0} {
      if {[catch {WPCmd PELdap results $ldapquery} results]} {
	WPCmd PEInfo statmsg "Some sort of ldap problem"
      }
    }

    cgi_head {
      WPStyleSheets
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post name=ldapaddr target=_top {
	cgi_text "page=ldappick" type=hidden notab
	cgi_text "ldapquery=$ldapquery" type=hidden notab
	cgi_text "field=$field" type=hidden notab
	cgi_text "addresses=[cgi_unquote_input $addresses]" type=hidden notab
	cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab

	cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {
	  cgi_table_row {
	    # next comes the menu down the left side
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu ldapres_cmds {}
	      }
	    }

	    cgi_table_data valign=top width=100% class=dialog {
	      if {$ldapquery == 0} {
		cgi_puts [cgi_italic "No matches found"]
	      } else {
		cgi_table align=center width="75%" {
		  cgi_table_row {
		    cgi_table_data align=center valign=middle {
		      cgi_br
		      cgi_put "Below are directory entries matching text expanded from your message's $field field. Choose the addressees you want, then Click [cgi_bold Address]."
		      cgi_br
		      cgi_br
		    }
		  }
		}

		set numboxes 0
		set srchindex 0
		cgi_table border=0 cellpadding=0 cellspacing=0 align=center width=96% {
		  foreach searchres $results {
		    set srchstr [lindex $searchres 0]
		    set retdata [lindex $searchres 1]
		    set expstr ""
		    cgi_table_row {
		      cgi_table_data {
			
			if {$srchstr != ""} {
			  set expstr " for \"[cgi_bold $srchstr]\""
			}

			cgi_text "str${srchindex}=${srchstr}" type=hidden notab
			cgi_table border=0 cellspacing=0 cellpadding=1 width=100% {
			  cgi_table_row {
			    cgi_table_data colspan=32 valign=middle height=20 class=ops {
			      cgi_puts "[cgi_nbspace]Directory Search Results${expstr}"
			    }
			  }

			  set whitebg 1
			  set nameindex 0
			  set onetruebox 0
			  set numsrchboxes 0
			  foreach litem $retdata {
			    if {[llength [lindex $litem 4]] > 0} {
			      incr numsrchboxes
			      if {$numsrchboxes > 1} {
				break
			      }
			    }
			  }
			  if {$numsrchboxes == 1} {
			    set onetruebox 1
			  }
			  foreach litem $retdata {
			    set name [lindex $litem 0]
			    set email [lindex $litem 4]
			    set nomail 0
			    if {$whitebg == 1} {
			      set bgcolor #FFFFFF
			      set whitebg 0
			    } else {
			      set bgcolor #EEEEEE
			      set whitebg 1
			    }
			    if {[llength $email] < 1} {
			      incr nameindex
			      continue
			      set nomail 1
			    }
			    cgi_table_row bgcolor=$bgcolor {

				cgi_table_data valign=top nowrap {
				  if {$nomail == 1} {
				    cgi_puts "&nbsp;"
				  } else {
				    if {$onetruebox == 1} {
				      set checked checked
				    } else {
				      set checked ""
				    }
				    cgi_checkbox "ldapList=${srchindex}.${nameindex}" style="background-color:$bgcolor" $checked
				    incr numboxes
				  }
				}

			      cgi_table_data valign=top nowrap {
				regsub -all "<" $name "\\&lt;" name
				regsub -all ">" $name "\\&gt;" name
				cgi_puts "$name"
			      }
			      cgi_table_data valign=top nowrap {
				if {[llength $email] > 1} {
				  cgi_table width=100% {
				    foreach eaddr $email {
				      cgi_table_row {
					cgi_table_data {
					  cgi_puts [cgi_font size=-1 "style=font-family:courier_new,monospace" "[cgi_lt]${eaddr}[cgi_gt]"]
					}
				      }
				    }
				  }
				} else {
				  if {$nomail == 1} {
				    cgi_puts "\[No email information\]"
				  } else {
				    cgi_puts [cgi_font size=-1 "style=font-family:courier_new,monospace" "[cgi_lt][lindex $email 0][cgi_gt]"]
				  }
				}
			      }
			    }
			    incr nameindex
			  }
			}

			cgi_br
			cgi_br
		      }
		    }
		    incr srchindex
		  }
		}
		cgi_text "numboxes=${numboxes}" type=hidden notab
	      }
	    }
	  }
	}
      }
    }
  }
}

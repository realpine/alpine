#!./tclsh
# $Id: cledit.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#
#  cledit.tcl
#
#  Purpose:  CGI script to generate html form editing of a single collection

# read config
source ./alpine.tcl
source cmdfunc.tcl

set cledit_vars {
  {cid     "Missing Command ID"}
  {oncancel "Missing oncancel"}
  {cl      {}  0}
  {add     {}  0}
  {errtext {}  0}
  {nick    {}  ""}
  {server  {}  ""}
  {ssl     {}  1}
  {user    {}  ""}
  {stype   {}  "imap"}
  {path    {}  ""}
  {view    {}  ""}
  {onclecancel {} ""}
}

## read vars
foreach item $cledit_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      if {[llength $item] > 2} {
	set [lindex $item 0] [lindex $item 2]
      } else {
	error [list _action [lindex $item 1] $result]
      }
    }
  } else {
    set [lindex $item 0] 1
  }
}

set cledit_menu {
  {
    {}
    {
      {
	# * * * * OK * * * *
	if {$add} {
	  cgi_image_button cle_save=[WPimg but_save] border=0 alt="Save Config"
	} else {
	  cgi_image_button cle_save=[WPimg but_save] border=0 alt="Save Config"
	}
      }
    }
  }
  {
    {}
    {
      {
	# * * * * CANCEL * * * *
	if {[string length $onclecancel]} {
	  cgi_puts [WPMenuURL "wp.tcl?page=$onclecancel&cid=$cid&oncancel=$oncancel" "" [cgi_img [WPimg but_cancel] border=0 alt="Cancel"] "" target=_top]
	} else {
	  cgi_puts [WPMenuURL "wp.tcl?page=$oncancel&cid=$cid" "" [cgi_img [WPimg but_cancel] border=0 alt="Cancel"] "" target=_top]
	}
      }
    }
  }
}

cgi_http_head {
  WPStdHttpHdrs
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "Collection List Configuration"
    WPStyleSheets
  }

  cgi_body BGCOLOR="$_wp(bordercolor)" {

    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=clconfig target=_top {
      cgi_text "cid=$cid" type=hidden notab
      cgi_text "page=conf_process" type=hidden notab
      cgi_text "cp_op=clconfig" type=hidden notab
      cgi_text "oncancel=$oncancel" type=hidden notab
      cgi_text "cl=$cl" type=hidden notab
      cgi_text "add=$add" type=hidden notab
      cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	cgi_table_row {
	  #
	  # next comes the menu down the left side
	  #
	  eval {
	    cgi_table_data $_wp(menuargs) rowspan=4 {
	      WPTFCommandMenu cledit_menu {}
	    }
	  }
	}

	#
	# In main body of screen goe confg list
	#
	cgi_table_row {
	  cgi_table_data valign=top width="100%" class=dialog {
	    if {$add == 0 && $errtext == 0} {
	      set item [WPCmd PEConfig clextended $cl]
	      set nick [lindex $item 0]
	      set server [lindex $item 2]
	      set path [lindex $item 3]
	      set view [lindex $item 4]
	      set user ""
	      set stype "imap"
	      set ssl 0
	    }
	    set server_flags {
	      ssl
	      user
	      imap
	    }
	    # jpf: took out nntp, service, and pop3
	    foreach flag $server_flags {
	      if {[regexp -nocase "^\[^/\]*(/\[^/\]*)*(/$flag\[^/\]*)(/\[^/\]*)*" $server match junk1 flagset junk2]} {
		set regprob 1
		if {[regexp -nocase "^/$flag\$" $flagset]} {
		  set regprob 0
		  if {[string compare [string tolower $flag] "ssl"] == 0} {
		    set ssl 1
		  } elseif {[regexp -nocase "(imap|nntp|pop3)" $flag]} {
		    set stype "$flag"
		  } else {
		    set regprob 1
		  }
		} elseif {[regexp -nocase "^/user=(.*)\$" $flagset match tuser]} {
		  set user "$tuser"
		  set regprob 0
		} elseif {[regexp -nocase "^/service=(.*)\$" $flagset match tuser]} {
		  if {[regexp -nocase "(imap|nntp|pop3)" $tuser]} {
		    set stype "$stype"
		  } else {set regprob 1}
		} else {
		  set regprob 1
		}
		if {$regprob == 0} {
		  regsub -nocase "^(\[^/\]*)(/\[^/\]*)*(/$flag\[^/\]*)(/\[^/\]*)*" $server "\\1\\2\\4" server
		}
	      }
	    }
	    cgi_table border=0 cellspacing=0 cellpadding=5 {
	      cgi_table_row {
		cgi_table_data align=right {
		  cgi_puts "[cgi_font size=+1 "Nickname[cgi_nbspace]: "]"
		}
		cgi_table_data align=left {
		  cgi_text "nick=$nick" size=40 notab
		}
	      }
	      cgi_table_row {
		cgi_table_data align=right valign=top {
		  cgi_puts "[cgi_font size=+1 "Server[cgi_nbspace]: "]"
		}
		cgi_table_data align=left {
		  cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" {
		    cgi_table_row {
		      cgi_table_data colspan=2 {
			cgi_text "server=$server" size=40 notab
		      }
		    }
		    cgi_table_row {
		      cgi_table_data {
			cgi_puts "[cgi_font size=-1 "SSL"]"
			if {$ssl == 1} {
			  set checked checked
			} else {
			  set checked ""
			}
			cgi_checkbox "ssl" style=background-color:$_wp(dialogcolor) $checked
		      }
		      cgi_table_data align=right {
			cgi_puts "[cgi_font size=-1 "User:[cgi_nbspace]"]"
			cgi_text "user=$user" size=10 notab
		      }
		      cgi_text "stype=imap" type=hidden notab
		    }
		  }
		}
	      }
	      cgi_table_row {
		cgi_table_data align=right {
		  cgi_puts "[cgi_font size=+1 "Path[cgi_nbspace]: "]"
		}
		if {[string compare "nntp" $stype] == 0 && [regexp -nocase {^#news\.(.*)$} $path match rempath]} {
		  set path "$rempath"
		}
		cgi_table_data align=left {
		  cgi_text "path=$path" size=40 notab
		}
	      }
	      cgi_table_row {
		cgi_table_data align=right {
		  cgi_puts "[cgi_font size=+1 "View[cgi_nbspace]: "]"
		}
		cgi_table_data align=left {
		  cgi_text "view=$view" size=40 notab
		}
	      }
	    }
	  }
	}
	
	cgi_table_row {
	  cgi_table_data height=200 class=dialog {
	    cgi_puts [cgi_nbspace]
	  }
	}
      }
    }
  }
}

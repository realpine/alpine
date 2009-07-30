#!./tclsh
# $Id: ldapquery.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  ldapquery.tcl
#
#  Purpose:  CGI script to handle ldap query

#  Input:
set ldap_vars {
  {dir		"Missing Directory Index"}
  {srchstr	{}	""}
  {field	{}	""}
  {op		{}	""}
  {searchtype	{}	""}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl


set ldap_menu {
}

set common_menu {
  {
    {}
    {
      {
	# * * * * Ubiquitous INBOX link * * * *
	if {[string compare inbox [string tolower [WPCmd PEMailbox mailboxname]]]} {
	  cgi_put [cgi_url INBOX open.tcl?folder=INBOX&colid=0&cid=[WPCmd PEInfo key] target=_top class=navbar]
	} else {
	  cgi_put [cgi_url INBOX fr_main.tcl target=_top class=navbar]
	}
      }
    }
  }
  {
    {}
    {
      {
	# * * * * FOLDER LIST * * * *
	cgi_puts [cgi_url "Folder List" "wp.tcl?page=folders&cid=[WPCmd PEInfo key]" target=_top class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * COMPOSE * * * *
	cgi_puts [cgi_url Compose wp.tcl?page=compose&oncancel=addrbook&cid=[WPCmd PEInfo key] target=_top class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * RESUME * * * *
	cgi_puts [cgi_url Resume wp.tcl?page=resume&oncancel=addrbook&cid=[WPCmd PEInfo key] class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * Addr books * * * *
	cgi_puts [cgi_url "Address Book" wp.tcl?page=addrbook&oncancel=main.tcl target=_top class=navbar]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {}
    {
      {
	# * * * * HELP * * * *
	cgi_puts [cgi_url "Get Help" "wp.tcl?page=help&oncancel=addrbook" target=_top class=navbar]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {}
    {
      {
	# * * * * QUIT * * * *
	cgi_puts [cgi_url "Quit Web Alpine" "$_wp(serverpath)/session/logout.tcl?cid=[WPCmd PEInfo key]&sessid=$sessid class=navbar" target=_top class=navbar]
      }
    }
  }
}


WPEval $ldap_vars {
  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      cgi_http_equiv Refresh "0; url=$_wp(serverpath)/$_wp(appdir)/$_wp(ui1dir)/ldapresult.tcl?dir=${dir}&srchstr=[WPPercentQuote ${srchstr}]&field=${field}&op=${op}&searchtype=${searchtype}&sessid=${sessid}"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {

      cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	cgi_table_row {
	  cgi_table_data valign=top rclass=navbar {
	    WPTFCommandMenu {} {}
	  }

	  cgi_table_data valign=top bgcolor=#ffffff width=100% {
	    cgi_table border=0 width=500 cellpadding=3 {
	      cgi_table_row {
		cgi_table_data align=center "style=\"padding-top:120px\"" {
		  cgi_put "[cgi_font "style=\"font-family: arial, sans-serif; font-size:18pt; font-weight: bold\"" "Searching Directory "][cgi_img [WPimg dotblink]]"
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

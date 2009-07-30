# $Id: addrbook.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  addrbook.tcl
#
#  Purpose:  CGI script to generate html output associated with address
#	     book entry and collection management
#
#  Input:
set abook_vars {
  {op		{}	"view"}
  {field	{}	"none"}
  {uid		""	0}
  {oncancel	""	"fr_main.tcl"}
  {reload}
}

#  Output:
#
#	HTML/CSS data representing the address book


# Command Menu definition for Message View Screen
set addr_menu {
}

set common_menu {
  {
    {expr {$view}}
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
    {expr {$view}}
    {
      {
	# * * * * FOLDER LIST * * * *
	cgi_puts [cgi_url "Folder List" "wp.tcl?page=folders&cid=[WPCmd PEInfo key]" target=_top class=navbar]
      }
    }
  }
  {
    {expr {$view}}
    {
      {
	# * * * * COMPOSE * * * *
	cgi_puts [cgi_url Compose wp.tcl?page=compose&oncancel=addrbook&cid=[WPCmd PEInfo key] target=_top class=navbar]
      }
    }
  }
  {
    {expr {$view}}
    {
      {
	# * * * * RESUME * * * *
	cgi_puts [cgi_url Resume wp.tcl?page=resume&oncancel=addrbook&cid=[WPCmd PEInfo key] class=navbar]
      }
    }
  }
  {
    {expr {$browse}}
    {
      {
	# * * * * USE ADDRESSES * * * *
	cgi_submit_button "address=Address" class="navtext"
      }
    }
  }
  {
    {expr {$browse}}
    {
      {
	# * * * * CANCEL * * * *
	cgi_submit_button "cancel=Cancel" class="navtext"
      }
    }
  }
  {
    {expr {0 && $browse}}
    {
      {
	# * * * * Address/Cancel * * * *
	cgi_submit_button doit=Done class="navbar"
	cgi_br
	cgi_select addrop class=navtext {
	  cgi_option "Action..." value=null
	  cgi_option Address value=address
	  cgi_option Cancel value=cancel
	}
      }
    }
  }
}


## read vars
foreach item $abook_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

# perform any requested actions

# preserve vars that my have been overridden with cgi parms

set cid [WPCmd PEInfo key]
WPCmd PEAddress safecheck

set view [expr {[string compare $op "view"] == 0}]
set browse [expr {[string compare $op "browse"] == 0}]

if {$view} {
  if {[catch {WPNewMail $reload} newmail]} {
    error [list _action "new mail" $newmail]
  }

  if {[WPCmd PEInfo ldapenabled] == 1} {
    if {[catch {WPCmd PELdap directories} directories] || [llength $directories] <= 0} {
      catch {unset directories}
    } else {
      for {set i 0} {$i < [llength $directories]} {incr i} {
	lappend exclusions document.ldapsearch${i}.srchstr
      }
    }
  }
}

# paint the page
cgi_http_head {
  WPStdHttpHdrs
}

cgi_html {
  cgi_head {
    set onload "onLoad="
    set onunload "onUnload="

    if {[info exists _wp(exitonclose)] && $view} {
      WPExitOnClose
      append onload "wpLoad();"
      append onunload "wpUnLoad();"
    }

    if {$view} {
      set normalreload [cgi_buffer {WPHtmlHdrReload "[file join $_wp(appdir) $_wp(ui1dir) wp.tcl?page=addrbook]"}]
      if {[info exists _wp(exitonclose)]} {
	WPStdHtmlHdr "Address Book View"
	cgi_script  type="text/javascript" language="JavaScript" {
	  cgi_put  "function indexReloadTimer(t){"
	  cgi_put  " reloadtimer = window.setInterval('wpLink(); window.location.replace(\\'[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=addrbook&reload=1\\')', t * 1000);"
	  cgi_puts "}"
	}

	append onload "indexReloadTimer($_wp(refresh));"

	cgi_noscript {
	  cgi_puts $normalreload
	}
      } else {
	cgi_puts $normalreload
      }
    }

    WPStyleSheets
    cgi_puts  "<style type='text/css'>"
    if {$browse} {
      cgi_puts  ".navbtn	{ color: white ;  font-family: geneva, arial, sans-serif ; font-size: 9pt ; letter-spacing: 0pt ; text-decoration: underline ; background: transparent; border: 0 ; text-align: left }"
    } elseif {$view} {
      cgi_puts  ".gradient	{ background-image: url('[WPimg indexhdr]') ; background-repeat: repeat-x }"
    }
    cgi_puts  "</style>"

    if {$_wp(keybindings)} {
      set kequiv {
	{{i} {top.location = 'fr_main.tcl'}}
	{{l} {top.location = 'wp.tcl?page=folders'}}
	{{?} {top.location = 'wp.tcl?page=help&oncancel=addrbook'}}
      }

      lappend kequiv [list {c} "top.location = 'wp.tcl?page=compose&oncancel=addrbook&cid=$cid'"]

      if {![info exists exclusions]} {
	set exclusions ""
      }

      append onload [WPTFKeyEquiv $kequiv $exclusions]
    }
  }

  cgi_body bgcolor=$_wp(bordercolor) background=[file join $_wp(imagepath) logo $_wp(logodir) back.gif] "style=\"background-repeat: repeat-x\"" $onload $onunload {

    if {$view} {
      catch {WPCmd PEInfo set help_context addrbook}
    } else {
      catch {WPCmd PEInfo set help_context addrbrowse}
    }

    set books [WPCmd PEAddress books]
    set entrylist {}
    set entryexists 0
    foreach book $books {
      set entries [WPCmd PEAddress list [lindex $book 0]]
      if {[llength $entries] > 0} {
	incr entryexists
      }
      lappend entrylist $entries
    }

    if {$view} {
      WPTFTitle "Address Books" $newmail 0 addrbook
    }

    cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {
      if {$browse} {
	cgi_puts "<form action=\"[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl\" name=addrchoice method=get target=_top>"
	cgi_text "page=addrpick" type=hidden notab
	cgi_text "field=$field" type=hidden notab
	cgi_text "restore=1" type=hidden notab
	cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
      }
      cgi_table_row {
	#
	# next comes the menu down the left side
	#
	cgi_table_data valign=top rowspan=4 class=navbar {
	  cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=0 width=112 {
	    if {!$browse} {
	      cgi_table_row {
		cgi_table_data class=navbar "style=\"padding: 6 0 6 4\"" {
		  cgi_puts [cgi_span "style=font-weight: bold" "Current Folder"]
		  cgi_division align=center "style=\"margin-top:4;margin-bottom:4\"" {
		    set mbn [WPCmd PEMailbox mailboxname]
		    if {[string length $mbn] > 16} {
		      set mbn "[string range $mbn 0 14]..."
		    }

		    cgi_put [cgi_url $mbn fr_main.tcl target=_top class=navbar]
		    switch -exact -- [WPCmd PEMailbox state] {
		      readonly {
			cgi_br
			#cgi_put [cgi_span "style=color: black; border: 1px solid red; background-color: pink; font-weight: bold" "Read Only"]
			cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Read Only)"]
		      }
		      closed {
			cgi_br
			#cgi_put [cgi_span "style=color: black; border: 1px solid red; background-color: pink; font-weight: bold" "Closed"]
			cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Closed)"]
		      }
		      ok -
		      default {}
		    }

		    cgi_br
		  }

		  cgi_hr "width=75%"
		}
	      }
	    }

	    if {$view && [llength $books] == 1 && [lindex [lindex $books 0] 3]} {
	      cgi_table_row {
		cgi_table_data class=navbar "style=\"padding-left: 4\"" {
		  # * * * * ADD ENTRY * * * *
		  cgi_puts [cgi_url "Add Entry" wp.tcl?page=addredit&add=1&book=0&cid=[WPCmd PEInfo key] class=navbar]
		}
	      }
	      cgi_table_row {
		cgi_table_data class=navbar {
		  cgi_put [cgi_nbspace]
		}
	      }
	    }

	    # next comes the menu down the left side, with suitable
	    cgi_table_row {
	      if {$view} {
		lappend common_menu {{cgi_put [cgi_nbspace]}}
		lappend common_menu [list {} [list {cgi_puts [cgi_url "Configure" wp.tcl?page=conf_process&newconf=1&oncancel=addrbook&cid=[WPCmd PEInfo key] class=navbar target=_top]}]]
		lappend common_menu [list {} [list {cgi_puts [cgi_url "Get Help" wp.tcl?page=help&oncancel=addrbook target=_top class=navbar]}]]
		lappend common_menu {{cgi_put [cgi_nbspace]}}

		if {[WPCmd PEInfo feature quit-without-confirm]} {
		  lappend common_menu [list {} [list {cgi_puts [cgi_url "Quit $_wp(appname)" $_wp(serverpath)/session/logout.tcl?cid=[WPCmd PEInfo key]&sessid=$sessid class=navbar]}]]
		} else {
		  lappend common_menu [list {} [list {cgi_puts [cgi_url "Quit $_wp(appname)" wp.tcl?page=quit&cid=[WPCmd PEInfo key] target=_top class=navbar]}]]
		}
	      }


	      eval {
		cgi_table_data $_wp(menuargs) class=navbar "style=\"padding: 0 0 10 2\"" {
		  WPTFCommandMenu addr_menu common_menu
		}
	      }
	    }
	  }
	}

	cgi_table_data valign=top width=100% class=dialog {
	  set n 1
	  set bookno 0
	  foreach book $books {
	    cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" {
	      if {$browse} {
		cgi_table_row {
		  cgi_table_data width="100%" colspan=16 class=dialog align=center valign=middle {
		    cgi_table cellpadding=4 width="70%" {
		      cgi_table_row {
			cgi_table_data align=center {
			  cgi_puts "Check the box next to the addresses you'd like to add to the [cgi_bold "[string toupper [string range $field 0 0]][string range $field 1 end]:"] field, then click [cgi_italic Address] button."
			}
		      }
		    }
		  }
		}
	      }

	      set format [WPCmd PEAddress format [lindex $book 0]]
	      set name [lindex $book 1]
	      set readwrite [lindex $book 3]
	      if {[llength $books] > 1} {
		if {[string index $name 0] == "\{"} {
		  if {$readwrite} {
		    set name "Remote Address Book"
		  } else {
		    set name "Global Address Book"
		  }
		}
		
		cgi_table_row {
		  cgi_table_data height=35 valign=middle colspan=[expr ([llength $format] + 1) * 2] nowrap {
		    if {$readwrite || $browse} {
		      cgi_puts "[cgi_font size=+1 [cgi_bold $name]]"
		    } else {
		      cgi_table border=0 cellspacing=0 cellpadding=0 width=100% {
			cgi_table_row {
			  cgi_table_data align=left {
			    cgi_puts "[cgi_font size=+1 [cgi_bold $name]]"
			  }
			  cgi_table_data align=right {
			    cgi_puts "[cgi_bold "(Read Only)"]"
			  }
			}
		      }
		    }
		  }
		}
		
		if {$view && $readwrite} {
		  cgi_table_row {
		    cgi_table_data nowrap height=40 {
		      cgi_put [cgi_url "Add New Entry" wp.tcl?page=addredit&add=1&book=${bookno}&cid=[WPCmd PEInfo key]]
		    }
		  }
		}
	      }
	      
	      cgi_table_row {
		cgi_table_data {
		  cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" {
		    set linenum 1
		    set nonick 1
		    set nfields [llength $format]

		    if {$view} {
		      cgi_table_row class=\"gradient\" {
			foreach f $format {
			  cgi_table_data {
			    cgi_put [cgi_img [WPimg dot2] border=0 width=3 height=24]
			  }
			  cgi_table_data align=left class=indexhdr {
			    switch [lindex $f 0] {
			      nick {
				cgi_put "Nickname (Click to edit entry)"
			      }
			      full {
				cgi_put "Full Name"
			      }
			      addr {
				cgi_put "Address (Click to Compose To)"
			      }
			    }
			  }
			}
		      }
		    }

		    for {set i 0} {$i < $nfields && $nonick} {incr i} {
		      if {[string compare "nick" [lindex [lindex $format $i] 0]] == 0} {
			set nonick 0
		      }
		    }
		    set entries [lindex $entrylist [lindex $book 0]]
		    if {[llength $entries] == 0} {
		      cgi_table_row {
			cgi_table_data colspan=16 align=center {
			  cgi_puts [cgi_italic "This address book is currently empty."]
			}
		      }
		    }
		    set aindex 0
		    foreach entry $entries {
		      if {[incr linenum] % 2} {
			set bgcolor #ffffff
		      } else {
			set bgcolor #eeeeee
		      }
		      cgi_table_row bgcolor=$bgcolor {
			set nick [lindex [lindex $entry 0] 0]
			set safenick [WPPercentQuote $nick]

			set nfields [llength $format]
			if {$browse} {
			  cgi_table_data valign=top nowrap {
			    cgi_checkbox "nickList=[lindex $book 0].$aindex.[lindex [lindex $entry 0] 0]" style="background-color:$bgcolor"
			  }
			}
			for {set i 0} {$i < $nfields} {incr i} {
			  set field [lindex $format $i]
			  set data [lindex $entry [expr $i + 1]]
			  
			  if {$view} {
			    cgi_table_data nowrap {
			      if {$nonick && $i == 0} {
				set data [cgi_url $data "wp.tcl?page=addredit&nick=${safenick}&book=${bookno}&ai=${aindex}"]
			      }
			      cgi_puts [cgi_nbspace][cgi_nbspace][cgi_nbspace][cgi_nbspace]
			    }
			  }
			  
			  cgi_table_data valign=top nowrap {
			    switch -- [lindex $field 0] {
			      addr {
				switch -- [lindex [lindex $entry 0] 1] {
				  single {
				    regsub -all "<" $data "\\&lt;" data
				    regsub -all ">" $data "\\&gt;" data
				    if {$view} {
				      set data [cgi_url $data "wp.tcl?page=compose&nickto=${safenick}&book=${bookno}&ai=${aindex}&oncancel=addrbook&cid=[WPCmd PEInfo key]"]
				    }
				    cgi_puts [cgi_font size=-1 face=courier $data]										
				  }
				  list {
				    cgi_table {
				      cgi_table_row {
					cgi_table_data {
					  foreach addr $data {
					    regsub -all "<" $addr "\\&lt;" addr
					    regsub -all ">" $addr "\\&gt;" addr
					    if {$view} {
					      set addr [cgi_url $addr "wp.tcl?page=compose&nickto=${safenick}&book=${bookno}&ai=${aindex}&oncancel=addrbook&cid=[WPCmd PEInfo key]"]
					    }
					    cgi_puts [cgi_font size=-1 face=courier $addr]
					    cgi_br
					  }
					  if {$view} {
					    cgi_puts "</a>"
					  }
					}
				      }
				    }
				  }
				  default {
				    cgi_puts "Unknown entry type"
				  }
				}
			      }
			      nick {
				regsub -all "<" $data "\\&lt;" data
				regsub -all ">" $data "\\&gt;" data
				if {$view} {
				  if {[string length $data]} {
				    set text "$data"
				  } else {
				    set text "\[Edit\]"
				  }

				  cgi_puts [cgi_url $text "wp.tcl?page=addredit&nick=${safenick}&book=${bookno}&ai=${aindex}"]
				} else {
				  if {![string length $nick]} {
				    set nick [cgi_nbspace]
				  }

				  cgi_puts "$nick"
				}
			      }
			      default {
				regsub -all "<" $data "\\&lt;" data
				regsub -all ">" $data "\\&gt;" data
				if {[string compare "$data" ""] == 0} {
				  cgi_puts "&nbsp;"
				} else {
				  cgi_puts "$data"
				}
			      }
			    }
			  }
			}
		      }
		      incr aindex
		    }
		  }
		}
	      }
	    }
	    incr bookno
	  }
	  if {$browse} {
	    cgi_puts "</form>"
	  }

	  if {[info exists directories]} {
	    cgi_table border=0 cellspacing=0 cellpadding=4 width="100%" "style=\"padding-top:10\"" {
	      for {set i 0} {$i < [llength $directories]} {incr i} {
		set directory [lindex $directories $i]
		set nick [lindex $directory 0]
		set server [lindex $directory 1]
		if {[string length $nick]} {
		  set ref $nick
		} elseif {[string length $server]} {
		  set ref "<$server>"
		} else {
		  set ref "some server"
		}

		cgi_table_row {
		  cgi_table_data colspan=3 valign=middle nowrap {
		    cgi_puts "[cgi_font size=+1 [cgi_bold "Directory server [cgi_quote_html $ref]"]]"
		  }
		}

		cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post enctype=multipart/form-data name=ldapsearch${i} {
		  cgi_text "sessid=$_wp(sessid)" type=hidden notab
		  cgi_text "page=ldapquery" type=hidden notab

		  cgi_text "searchtype=0" type=hidden notab
		  cgi_text "op=view" type=hidden notab
		  cgi_text "dir=$i" type=hidden notab

		  cgi_table_row {
		    cgi_table_data class=dialog align=center valign=middle "style=\"background-color:white\"" {
		      cgi_puts "Search Directory :"
		      cgi_text "srchstr=" size=35
		      cgi_submit_button "search=Search"
		    }
		  }
		}
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

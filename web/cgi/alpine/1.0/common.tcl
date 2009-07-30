#!./tclsh
# $Id: common.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  common.tcl
#
#  Purpose:  CGI script snippet to generate html output associated
#	     with the WebPine message view/index ops frame
#
#  Input:
set ops_vars {
}


# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set padleft 3px

#
# Command Menu definition for Message View Screen
#
set view_menu {
}

set common_menu {
  {
    {expr {0}}
    {
      {
	# * * * * UBIQUITOUS INBOX LINK * * * *
	if {[string compare inbox [string tolower [WPCmd PEMailbox mailboxname]]]} {
	  cgi_put [cgi_url INBOX open.tcl?folder=INBOX&colid=0&cid=[WPCmd PEInfo key] target=_top class=navbar]
	} else {
	  cgi_put [cgi_url INBOX fr_index.tcl target=spec class=navbar]
	}
      }
    }
  }
  {
    {expr {0}}
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
	cgi_puts [cgi_url Compose wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key] target=_top class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * RESUME * * * *
	cgi_puts [cgi_url Resume wp.tcl?page=resume&oncancel=main.tcl&cid=[WPCmd PEInfo key] target=_top class=navbar]
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
}


WPEval $ops_vars {
  cgi_http_head {
    WPStdHttpHdrs {} 60
  }

  cgi_html {
    cgi_head {
      if {[info exists _wp(exitonclose)]} {
	WPExitOnClose top.spec.body
      }

      WPStyleSheets
      cgi_script  type="text/javascript" language="JavaScript" {
	cgi_puts "function flip(n){"
	cgi_put  " var d = top.spec.body.document;"
	cgi_put  " var f = d.index;"
	cgi_put  " if(f && document.implementation){"
	cgi_put  "  var e = d.createElement('input');"
	cgi_put  "  var ver = navigator.appVersion;"
	cgi_put  "  if(!((ver.indexOf('MSIE')+1) && (ver.indexOf('Macintosh')+1))) e.type = 'hidden';"
	cgi_put  "  e.name = 'bod_'+n;"
	cgi_put  "  e.value = '1';"
	cgi_put  "  f.appendChild(e);"
	cgi_put  "  f.submit();"
	cgi_put  "  return false;"
	cgi_puts " }"
	cgi_puts " return true;"
	cgi_puts "}"
      }

      if {$_wp(keybindings)} {
	set kequiv {
	  {{l} {top.location = 'wp.tcl?page=folders'}}
	  {{a} {top.location = 'wp.tcl?page=addrbook'}}
	  {{?} {top.location = 'wp.tcl?page=help'}}
	  {{n} {if(flip('next')) top.spec.body.location = 'wp.tcl?page=body&bod_next=1'}}
	  {{p} {if(flip('prev')) top.spec.body.location = 'wp.tcl?page=body&bod_prev=1'}}
	  {{i} {if(top.spec.cmds) top.spec.location = 'fr_index.tcl'}}
	  {{s} {if(top.spec.cmds) top.spec.cmds.document.saveform.f_name.focus()}}
	  {{d} {if(top.spec.cmds) top.spec.cmds.document.delform.op[0].click()}}
	  {{u} {if(top.spec.cmds) top.spec.cmds.document.delform.op[1].click()}}
	  {{r} {if(top.spec.cmds) top.spec.cmds.document.replform.op.click()}}
	  {{f} {if(top.spec.cmds) top.spec.cmds.document.forwform.op.click()}}
	  {{ } {if(top.spec.body.document.index && flip('next')) top.spec.body.location = 'wp.tcl?page=body&bod_next=1'}}
	  {{-} {if(top.spec.body.document.index && flip('prev')) top.spec.body.location = 'wp.tcl?page=body&bod_prev=1'}}
	  {{z} {if(top.spec.body.document.index && top.spec.body.document.index.zoom) top.spec.body.document.index.zoom.click()}}
	}

	lappend kequiv [list {c} "top.location = 'wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key]'"]

	if {[WPCmd PEInfo feature enable-full-header-cmd]} {
	  lappend kequiv [list {h} "if(top.spec.cmds.document.saveform) top.spec.body.location = 'wp.tcl?page=view&fullhdr=flip'"]
	}

	set onload "onLoad=[WPTFKeyEquiv $kequiv {} top.spec.body] top.gen.focus();"
      }
    }

    cgi_body bgcolor=$_wp(bordercolor) $onload {

      cgi_put [cgi_url [cgi_imglink smalllogo] wp.tcl?page=help&topic=about class=navbar target=_top]

      cgi_br

      cgi_division class=navbar "style=\"background-color: $_wp(menucolor)\"" {
	cgi_division "style=\"padding: 8px 0 2px $padleft\"" {
	  cgi_puts [cgi_span "style=font-weight: bold" "Current Folder"]
	}

	set mbn [WPCmd PEMailbox mailboxname]
	if {[string length $mbn] > 16} {
	  set mbn "[string range $mbn 0 14]..."
	}

	cgi_division align=center {
	  cgi_put [cgi_url $mbn fr_index.tcl target=spec class=navbar]

	  switch -exact -- [WPCmd PEMailbox state] {
	    readonly {
	      cgi_br
	      cgi_put [cgi_span "style=color: yellow; font-weight: bold" "(Read Only)"]
	    }
	    closed {
	      cgi_br
	      cgi_put [cgi_span "style=color: yellow; font-weight: bold" "(Closed)"]
	    }
	    ok -
	    default {}
	  }
	}

	cgi_hr "width=75%" "style=\"margin-top:8px\""

	# Common Navigation controls
	cgi_division align=center "style=\"padding-bottom: 4px\"" {
	  cgi_put [cgi_img [WPimg but_rnd_block] border=0 "usemap=#nav" "alt=Navigation Controls"]
	  cgi_map nav {
	    cgi_area shape=rect coords=0,0,37,38 "href=wp.tcl?page=body&bod_prev=1" target=body "onClick=\"return flip('prev')\"" "alt=Previous"
	    cgi_area shape=rect coords=0,40,32,74 "href=wp.tcl?page=body&bod_next=1" target=body "onClick=\"return flip('next')\"" "alt=Next"
	    cgi_area shape=rect coords=50,0,82,38 "href=wp.tcl?page=body&bod_first=1" target=body "onClick=\"return flip('first')\"" "alt=First"
	    cgi_area shape=rect coords=50,40,82,74 "href=wp.tcl?page=body&bod_last=1" target=body "onClick=\"return flip('last')\"" "alt=Last"
	  }

	  # Jump option
	  if {[WPCmd PEInfo feature enable-jump-cmd]} {
	    cgi_br
	    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=body name=goform "style=margin-top:6" {
	      cgi_text "page=body" type=hidden notab
	      cgi_text gonum= class=navtext size=4 maxlength=6 "onClick=this.select()"
	      if {0} {
		cgi_br
		cgi_submit_button "goto=Jump to Msg \#" class=navtext "style=margin-right:2;margin-top:6"
	      } else {
		cgi_submit_button "goto=Jump" class=navtext "style=margin-right:2"
	      }
	    }
	  }
	}

	cgi_hr "width=75%"

	cgi_division "style=\"padding: 0 0 6px $padleft\"" {

	  if {[catch {WPSessionState left_column_folders} fln]} {
	    set fln $_wp(fldr_cache_def)
	  }

	  if {$fln > 0} {
	    cgi_puts [cgi_span "style=font-weight: bold" "Folders"]
	    cgi_division "style=\"padding-left: 4px\"" {
	      # UBIQUITOUS INBOX LINK
	      if {[string compare inbox [string tolower [WPCmd PEMailbox mailboxname]]]} {
		cgi_put [cgi_url INBOX open.tcl?folder=INBOX&colid=0&cid=[WPCmd PEInfo key] target=_top class=navbar]
	      } else {
		cgi_put [cgi_url INBOX fr_index.tcl target=spec class=navbar]
	      }

	      set n 0
	      set fc [WPTFGetFolderCache]
	      for {set i 0} {$i < [llength $fc] && $i < $fln} {incr i} {
		set f [lindex $fc $i]

		if {0 == [catch {WPCmd PEFolder exists [lindex $f 0] [lindex $f 1]} result] && $result} {
		  cgi_br

		  set fn [lindex $f 1]
		  if {[string length $fn] > 15} {
		    set fn "...[string range $fn end-15 end]"
		  }

		  if {[string compare [lindex $f 1] [WPCmd PEMailbox mailboxname]]} {
		    cgi_put [cgi_url $fn "open.tcl?folder=[lindex $f 1]&colid=[lindex $f 0]&cid=[WPCmd PEInfo key]" target=_top class=navbar]
		  } else {
		    cgi_put [cgi_url $fn fr_index.tcl target=spec class=navbar]
		  }

		  if {[incr n] >= $fln} {
		    break
		  }
		}
	      }

	      cgi_br
	      cgi_puts [cgi_url "More Folders..." "wp.tcl?page=folders&cid=[WPCmd PEInfo key]" target=_top class=navbar]
	    }
	  } else {
	    cgi_puts [cgi_url "Folder List" "wp.tcl?page=folders&cid=[WPCmd PEInfo key]" target=_top class=navbar]
	  }
	}

	cgi_hr "width=75%"

	# Common Operations
	cgi_division "style=\"padding: 0 0 0 $padleft\"" {
	  # * * * * COMPOSE * * * *
	  cgi_puts [cgi_url Compose wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key] target=_top class=navbar]
	  cgi_br
	  # * * * * RESUME * * * *
	  cgi_puts [cgi_url Resume wp.tcl?page=resume&oncancel=main.tcl&cid=[WPCmd PEInfo key] target=_top class=navbar]
	  cgi_br
	  # * * * * Addr books * * * *
	  cgi_puts [cgi_url "Address Book" wp.tcl?page=addrbook&oncancel=main.tcl target=_top class=navbar]
	}

	cgi_division "style=\"padding: 12px 0 0 $padleft\"" {
	  cgi_put [cgi_url "Configure" wp.tcl?page=conf_process&newconf=1&oncancel=main.tcl&cid=[WPCmd PEInfo key] class=navbar target=_top]
	  cgi_br
	  cgi_put [cgi_url "Get Help" wp.tcl?page=help class=navbar target=_top]
	}

	cgi_division "style=\"padding: 12px 0 10px $padleft\"" {
	  if {[WPCmd PEInfo feature quit-without-confirm]} {
	    cgi_puts [cgi_url "Quit $_wp(appname)" $_wp(serverpath)/session/logout.tcl?cid=[WPCmd PEInfo key]&sessid=$sessid target=_top class=navbar]
	  } else {
	    cgi_puts [cgi_url "Quit $_wp(appname)" wp.tcl?page=quit&cid=[WPCmd PEInfo key] target=_top class=navbar]
	  }
	}
      }
    }
  }
}

# $Id: view.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  view.tcl
#
#  Purpose:  CGI script to generate html output associated
#            with a message's particular text displayed in 
#            the "body" frame

#  Input:
set view_vars {
    {message	{}	0}
    {first	{}	1}
    {top	{}	0}
    {bod_first	{}	0}
    {bod_prev	{}	0}
    {bod_next	{}	0}
    {bod_last	{}	0}
    {fullhdr	{}	""}
    {reload	{}	0}
    {uid	{}	0}
    {cid	{}	0}
    {goto	{}	""}
    {gonum	{}	0}
    {save	{}	""}
    {f_name	{}	""}
    {f_colid	{}	0}
    {takecancel {}	""}
    {savecancel {}	0}
    {auths	{}	0}
    {user	{}	""}
    {pass	{}	""}
    {create	{}	0}
    {flipdelete	{}	0}
    {delete	{}	""}
    {undelete	{}	""}
    {replyall	{}	""}
    {replytext	{}	""}
    {submitted	{}	0}
    {printable	{}	0}
    {op		{}	""}
    {showimg	{}	""}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

#set use_icon_for_status 1

set hacktoken __URL__TEXT__

## read vars
foreach item $view_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {[catch {WPCmd PEInfo key} webpine_key]} {
  error [list _action "command ID" $webpine_key]
}

# make sure message to be "viewed" is still available
catch {unset errstr}
if {[catch {WPCmd PEMailbox messagecount} messagecount]} {
  set errstr $messagecount
} elseif {$uid > 0 && [catch {WPCmd PEMessage $uid number} message]} {
  set errstr $message
} elseif {$message > 0 && $message <= $messagecount && [catch {WPCmd PEMailbox uid $message} uid]} {
  set errstr $uid
} elseif {!($message && $uid)} {
  set errstr "Unknown message number"
}

if {[info exists errstr]} {
  switch $op {
    Reply -
    Forward {
      catch {WPCmd set wp_spec_script fr_view.tcl}
      catch {WPCmd set uid $uid}
      source [WPTFScript main]
    }
    default {
      cgi_http_head {
	WPStdHttpHdrs
      }

      cgi_html {
	cgi_head {
	  WPHtmlHdrReload "$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=view"
	  WPStyleSheets
	}

	cgi_body bgcolor=#ffffff {
	  cgi_table border=0 width="100%" height="100%" {
	    cgi_table_row {
	      cgi_table_data width="20%" {
		cgi_put [cgi_img [WPimg dot2] border=0 height=1]
	      }
	      cgi_table_data bgcolor=#ffffff class=notice {
		cgi_puts "\[[cgi_nbspace]"
		if {[string first "PEMessage: UID " $errstr] >= 0} {
		  cgi_puts "The message referred to is no"
		  cgi_puts "longer available."
		  cgi_p
		  cgi_puts "An error has been detected that indicates you may"
		  cgi_puts "have WebPine running in multiple browser windows."
		  cgi_p
		  cgi_puts "Please close all other windows that are displaying"
		  cgi_puts "WebPine pages."
		} else {
		  cgi_puts "Message Unavailable: $errstr."
		}
		cgi_p
		cgi_puts "Click \"[cgi_url [WPCmd PEMailbox mailboxname] fr_index.tcl target=spec]\" in the column on the left to return to the updated Message List.[cgi_nbspace]\]"
	      }
	      cgi_table_data width="20%" {
		cgi_put [cgi_img [WPimg dot2] border=0 height=1]
	      }
	    }
	  }
	}
      }
    }
  }
} else {

  # make sure any caching doesn't screw this setting
  catch {WPCmd set wp_spec_script fr_view.tcl}

  set zoomed [WPCmd PEMailbox zoom]

  set onload "onLoad="
  set onunload "onUnload="

  if {[info exists _wp(exitonclose)]} {
    set closescript [cgi_buffer WPExitOnClose]
    append onload "wpLoad();"
    append onunload "wpUnLoad();"
  }

  # perform any requested actions
  if {$flipdelete == 1} {
    if {$cid != $webpine_key} {
      lappend newmail [list "Can't Delete: Invalid command ID!"]
    } elseif {[WPCmd PEMessage $uid flag deleted]} {
      if {[catch {WPCmd PEMessage $uid flag deleted 0} result]} {
	lappend newmail [list "Error UNdeleting $message: $result"]
      }
    } else {
      if {[catch {WPCmd PEMessage $uid flag deleted 1} result]} {
	lappend newmail [list "Error deleting $message: $result"]
      } else {
	# bug: handle delete-skips-deleted
	set message [WPCmd PEMailbox next [WPCmd PEMessage $uid number]]
	set uid [WPCmd PEMailbox uid $message]
      }
    }
  } elseif {[string compare "report spam" [string tolower $op]] == 0} {
    if {[info exists _wp(spamaddr)] && [string length $_wp(spamaddr)]} {
      if {$cid != $webpine_key} {
	lappend newmail [list "Can't Spamify: Invalid command ID!"]
      } else {
	if {[info exists _wp(spamfolder)] && [string length $_wp(spamfolder)]
	    && [catch {
	      set savedef [WPCmd PEMailbox savedefault]
	      if {[WPCmd PEFolder exists [lindex $savedef 0] $_wp(spamfolder)] == 0} {
		WPCmd PEFolder create [lindex $savedef 0] $_wp(spamfolder)
	      }

	      WPCmd PEMessage $uid save [lindex $savedef 0] $_wp(spamfolder)
	    } result]} {
	  lappend newmail [list "Error spamifying message $message: $result"]
	} elseif {[catch {WPCmd PEMessage $uid flag deleted 1} result]} {
	  lappend newmail [list "Error spamifying message $message: $result"]
	} else {
	  set n [WPCmd PEMessage $uid number]
	  if {[info exists _wp(spamsubj)]} {
	    set subj $_wp(spamsubj)
	  } else {
	    set subj ""
	  }

	  if {[catch {WPCmd PEMessage $uid spam $_wp(spamaddr) $subj} result]} {
	    lappend newmail [list "Can't Report Spam: $result"]
	  } else {
	    lappend newmail [list "Message $n reported as Spam and flagged for deletion"]
	  }
	}
      }
    }
  } elseif {$delete == 1 || [string compare delete [string tolower $op]] == 0} {
    if {$cid != $webpine_key} {
      lappend newmail [list "Can't Delete: Invalid command ID!"]
    } elseif {[catch {WPCmd PEMessage $uid flag deleted 1} result]} {
      lappend newmail [list "Error deleting message $message: $result"]
    } else {
      set n [WPCmd PEMessage $uid number]
      # lappend newmail [list "Message $n deleted"]
      # bug: handle delete-skips-deleted
      set message [WPCmd PEMailbox next $n]
      set uid [WPCmd PEMailbox uid $message]
      lappend newmail [list "Message $n flagged for deletion"]
    }
  } elseif {$delete == 0 || $undelete == 1 || [string compare undelete [string tolower $op]] == 0} {
    if {$cid != $webpine_key} {
      catch {WPCmd PEInfo statmsg "Invalid command ID!"}
    } elseif {[catch {WPCmd PEMessage $uid flag deleted 0}]} {
      lappend newmail [list "Error UNdeleting message $message"]
    } else {
      lappend newmail [list "Deletion mark for message [WPCmd PEMessage $uid number] removed"]
    }
  } elseif {[string length $goto]} {
    if {[regexp {^([0-9]+)$} $gonum n]} {
      if {$n > 0 && $n <= [WPCmd PEMailbox last]} {
	set message $n
	set uid [WPCmd PEMailbox uid $message]
      } else {
	lappend newmail [list "Jump value out of range: $gonum"]
      }
    } else {
      if {[string length $gonum]} {
	lappend newmail [list "Unrecognized Jump value: $gonum"]
      } else {
	lappend newmail [list "Enter message number, then click 'Jump'"]
      }
    }
  } elseif {$savecancel == 1 || [string compare cancel [string tolower $savecancel]] == 0} {
    lappend newmail [list "Save cancelled. Folder not created."]
  } elseif {[string compare browse [string tolower $op]] == 0} {
    _cgi_set_uservar onselect main
    _cgi_set_uservar oncancel main
    _cgi_set_uservar controls 2
    source [WPTFScript savebrowse]
    set nopage 1
  } elseif {[string compare take [string tolower $op]] == 0} {
    source [WPTFScript take]
    set nopage 1
  } elseif {[string compare "take address" [string tolower $op]] == 0} {
    _cgi_set_uservar take 1
    set addrs ""
    if {[catch {cgi_import taList}] == 0} {
      foreach ta $taList {
	if {[catch {cgi_import_as ${ta}p xp}]} {
	  set xp ""
	}

	if {[catch {cgi_import_as ${ta}m xm}]} {
	  set xm ""
	}

	if {[catch {cgi_import_as ${ta}h xh}]} {
	  set xh ""
	}

	lappend alist [list $xp $xm $xh]
      }

      switch [llength $alist] {
	0 {
	  _cgi_set_uservar addrs ""
	}
	1 {
	  set a [lindex $alist 0]
	  _cgi_set_uservar fn [lindex $a 0]
	  _cgi_set_uservar addrs [WPPercentQuote "[lindex $a 1]@[lindex $a 2]"]
	}
	default {
	  set addrs ""
	  foreach a $alist {
	    if {[string length $addrs]} {
	      append addrs ","
	    }

	    append addrs [WPPercentQuote "[lindex $a 0] <[lindex $a 1]@[lindex $a 2]>"]
	  }

	  _cgi_set_uservar addrs $addrs
	}
      }

      source [WPTFScript takeedit]
    } else {
      WPCmd PEInfo statmsg "Take Address cancelled. No addresses selected."
      source [WPTFScript main]
    }

    set nopage 1
  } elseif {[string compare cancel [string tolower $takecancel]] == 0} {
    source [WPTFScript main]
    set nopage 1
  } elseif {[string compare forward [string tolower $op]] == 0} {
    _cgi_set_uservar style Forward
    source [WPTFScript compose]
    set nopage 1
  } elseif {[string compare reply [string tolower $op]] == 0} {
    _cgi_set_uservar style Reply
    _cgi_set_uservar repqstr [WPCmd PEMessage $uid replyquote]
    source [WPTFScript compose]
    set nopage 1
  } elseif {[string compare save [string tolower $op]] == 0} {
    if {[catch {WPCmd PEMessage $uid save $f_colid $f_name} reason]} {
      #statmsg "Save failed: $reason"
      lappend newmail [list "Save failed: $reason"]
    } else {
      set savedef [WPTFSaveDefault $uid]
      if {[lindex $savedef 0] == $f_colid} {
	WPTFAddSaveCache $f_name
      }

      if {[WPCmd PEInfo feature save-will-not-delete] == 0} {
	if {[catch {WPCmd PEMessage $uid flag deleted 1} reason]} {
	  # statmsg "Cannot delete saved message: $reason"
	  lappend newmail [list "Cannot delete saved message: $reason"]
	}
      }
    }

  } elseif {$bod_first} {
    set u $uid
    set message [WPCmd PEMailbox first]
    set uid [WPCmd PEMailbox uid $message]
    if {$zoomed && $u == $uid} {
      lappend newmail [list "Already on first of the Marked messages"]
    }
  } elseif {$bod_prev} {
    set u $uid
    set message [WPCmd PEMailbox next [WPCmd PEMessage $uid number] -1]
    set uid [WPCmd PEMailbox uid $message]
    if {$zoomed && $u == $uid} {
      lappend newmail [list "Already on first of the Marked messages"]
    }
  } elseif {$bod_next} {
    set u $uid
    set message [WPCmd PEMailbox next [WPCmd PEMessage $uid number]]
    set uid [WPCmd PEMailbox uid $message]
    if {$zoomed && $u == $uid} {
      lappend newmail [list "Already on last of the Marked messages"]
    }
  } elseif {$bod_last} {
    set u $uid
    set message [WPCmd PEMailbox last]
    set uid [WPCmd PEMailbox uid $message]
    if {$zoomed && $u == $uid} {
      lappend newmail [list "Already on last of the Marked messages"]
    }
  }

  if {![info exists nopage]} {
    switch -exact -- [WPCmd PEMailbox state] {
      readonly {
	lappend newmail [list [cgi_span "style=color: black; margin: 2 ; border: 1px solid red; background-color: pink; font-weight: bold" "Deleted messages may reapper because [WPCmd PEMailbox mailboxname] is Read Only"]]
      }
      closed {
	lappend newmail [list [cgi_span "style=color: black; border: 1px solid red; background-color: pink; font-weight: bold" "Message body may be blank because [WPCmd PEMailbox mailboxname] is Closed"]]
      }
      ok -
      default {}
    }

    if {[catch {WPNewMail $reload ""} newmailmsg]} {
      error [list _action "new mail" $newmailmsg]
    } else {
      foreach i $newmailmsg {
	lappend newmail $i
      }

      if {[info exists newmail] == 0} {
	set newmail ""
      }
    }

    if {$uid <= 0} {
      lappend newmail [list "Message $message has UID 0!"]
    }

    if {[WPCmd PEInfo feature enable-full-header-cmd]} {
      if {[string length $fullhdr]} {
	if {[WPCmd PEInfo mode full-header-mode]} {
	  if {$fullhdr == 0 || [string compare $fullhdr "off"] == 0} {
	    WPCmd PEInfo mode full-header-mode 0
	  }
	} else {
	  if {$fullhdr == 1 || [string compare $fullhdr "on"] == 0} {
	    WPCmd PEInfo mode full-header-mode 2
	  }
	}
      }

      if {[WPCmd PEInfo mode full-header-mode]} {
	set hmode "fullhdr=off"
	set text [cgi_imglink nofullhdr]
      } else {
	set hmode "fullhdr=on"
	set text [cgi_imglink fullhdr]
      }

      set hmode_url [cgi_url $text "wp.tcl?page=body&$hmode" name=hmode target=body style=vertical-align:middle]
    }

    if {$uid} {
      # preserve any new uid val
      WPCmd set uid $uid
    }

    if {[catch {WPCmd PEMessage $uid charset} charset]
	|| [string length $charset] == 0
	|| [string compare us-ascii [string tolower $charset]] == 0} {
      set charset "ISO-8859-1"
    }

    catch {fconfigure stdout -encoding binary}

    cgi_http_head {
      WPStdHttpHdrs "text/html; charset=$charset"
    }

    cgi_html {
      cgi_head {

	cgi_http_equiv Content-Type "text/html; charset=$charset"

	set normalreload [cgi_buffer {WPHtmlHdrReload "$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=body&uid=$uid"}]
	if {[info exists _wp(exitonclose)]} {
	  cgi_puts $closescript

	  cgi_script  type="text/javascript" language="JavaScript" {
	    cgi_put  "function viewReloadTimer(t){"
	    cgi_put  " reloadtimer = window.setInterval('wpLink(); window.location.replace(\\'[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=body&reload=1\\')', t * 1000);"
	    cgi_puts "}"
	  }

	  append onload "viewReloadTimer($_wp(refresh));"
	  cgi_noscript {
	    cgi_puts $normalreload
	  }
	} else {
	  cgi_puts $normalreload
	}

	if {[info exists _wp(timing)]} {
	  cgi_script  type="text/javascript" language="JavaScript" {
	    cgi_puts "var loadtime = new Date();"
	    cgi_puts "var submitted = $submitted;"
	    cgi_puts "function fini() {var now = new Date(); window.status = 'Page loaded in '+(now.getTime() - loadtime.getTime())/1000+' seconds';}"
	  }

	  append onload "fini();"
	}

	WPStyleSheets

	if {$_wp(keybindings)} {
	  set kequiv {
	    {{?} {top.location = 'wp.tcl?page=help'}}
	    {{l} {top.location = 'wp.tcl?page=folders'}}
	    {{a} {top.location = 'wp.tcl?page=addrbook'}}
	    {{n} {top.spec.body.location = 'wp.tcl?page=view&bod_next=1'}}
	    {{p} {top.spec.body.location = 'wp.tcl?page=view&bod_prev=1'}}
	    {{i} {top.spec.location = 'fr_index.tcl'}}
	    {{s} {top.spec.cmds.document.saveform.f_name.focus()}}
	    {{d} {top.spec.cmds.document.delform.op[0].click()}}
	    {{u} {top.spec.cmds.document.delform.op[1].click()}}
	    {{r} {top.spec.cmds.document.replform.op.click()}}
	    {{f} {top.spec.cmds.document.forwform.op.click()}}
	  }

	  lappend kequiv [list {c} "top.location = 'wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key]'"]

	  if {[info exists hmode_url]} {
	    lappend kequiv [list {h} "top.spec.body.location = 'wp.tcl?page=view&$hmode'"]
	  }

	  append onload [WPTFKeyEquiv $kequiv]
	}
      }

      set fgcolor [set normal_fgcolor [WPCmd PEInfo foreground]]
      set bgcolor [WPCmd PEInfo background]

      cgi_body bgcolor=white $onload $onunload {
	catch {WPCmd set help_context view}

	set infont 0
	set inurl 0

	if {[llength $newmail]} {
	  cgi_division align=center "style=\"border: 1px solid black; background: $_wp(bordercolor)\"" {
	    WPTFStatusTable $newmail 1 "style=\"padding: 6px 0\""
	  }
	}

	cgi_table width="100%" border=0 cellpadding=2 cellspacing=0 {

	  # Context
	  cgi_table_row {
	    if {$zoomed} {
	      set marked " [cgi_bold marked]"
	      set c $zoomed
	    } else {
	      set marked ""
	      set c $messagecount
	    }

	    cgi_table_data align=left valign=middle class=context {
	      # write message number

	      if {[catch {WPCmd PEMailbox messagecount before [WPCmd PEMessage $uid number]} prior]} {
		set prior 0
	      }

	      if {[catch {WPCmd PEMailbox messagecount after [WPCmd PEMessage $uid number]} remaining]} {
		set remaining 0
	      }

	      if {$c == 1} {
		set msgnotext "Only${marked} Message in [WPCmd PEMailbox mailboxname]"
	      } elseif {$remaining == 0} {
		set msgnotext "[cgi_bold Last] of [WPcomma $c]${marked} message[WPplural $c]"
	      } elseif {$prior == 0} {
		set msgnotext "[cgi_bold First] of [WPcomma $c]${marked} message[WPplural $c]"
	      } else {
		if {[string length $marked]} {
		  append marked " messages"
		  set n [expr {$zoomed - $remaining}]
		  set msgnotext "Message [WPcomma $message] ($n of [WPcomma $c] [cgi_bold marked] messages)"
		} else {
		  set msgnotext "Message [WPcomma $message] of [WPcomma $c]"
		}
	      }

	      if {[info exists use_icon_for_status]} {
		set staticon "[WPStatusImg $uid] "
		set stattext ""
	      } else {
		set staticon ""
		switch -regexp [lindex [WPStatusIcon $uid] 0] {
		  del {
		    set stattext " (Deleted)"
		  }
		  new {
		    set stattext " (New)"
		  }
		  default {
		    set stattext ""
		  }
		}
	      }

	      cgi_put "${staticon}${msgnotext}${stattext}"
	    }

	    if {$printable} {
	      cgi_table_data align=right valign=top bgcolor=#bfbfbf class=context {
		cgi_puts "Folder: [WPCmd PEMailbox mailboxname]"
	      }
	    } else {
	      if {[info exists common_goto_in_view_specific_frame]} {
		cgi_table_data align=right valign=top bgcolor=#bfbfbf class=context {
		  cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get target=body {
		    cgi_text "page=view" type=hidden notab
		    cgi_text gonum= type=text size=4 maxlength=4 class=navtext
		    cgi_submit_button "goto=Jump to Msg" class=navtext
		    cgi_put [cgi_nbspace]
		  }
		}
	      }

	      cgi_table_data align=right valign=top bgcolor=#bfbfbf class=context {
		cgi_put [cgi_url [cgi_img [WPimg printer2] border=0 style=vertical-align:baseline] wp.tcl?page=view&uid=$uid&printable=1 target=_blank "onClick=if(window.print){window.print();return false;}else return true;"]
		cgi_put [cgi_img [WPimg dot2] height=1 width=4]
		if {[info exists hmode_url]} {
		  cgi_put $hmode_url
		} else {
		  cgi_put [cgi_imglink dot]
		}
	      }
	    }
	  }
	  

	  cgi_table_row {
	    cgi_table_data valign=top bgcolor=#${bgcolor} class=view width="100%" colspan=2 {
	      cgi_preformatted "style=\"color:#$normal_fgcolor\"" {
		#cgi_division "style=\"color: #$normal_fgcolor; font-family: courier\""

		set msgtext [WPCmd PEMessage $uid text]

		# pre-scan message text for anything interesting
		foreach i $msgtext {
		  foreach j $i {
		    switch -exact [lindex $j 0] {
		      img {
			if {![info exists showimages]} {
			  # ONLY IMG tags in HTML text that reference ATTACHED IMAGE files are allowed
			  if {[catch {WPCmd PEMessage $uid cid "<[lindex [lindex $j 1] 0]>"} cidpart] == 0
			      && [string length $cidpart]
			      && [catch {WPCmd PEMessage $uid attachinfo $cidpart} attachinfo] == 0
			      && [string compare [string tolower [lindex $attachinfo 1]] image] == 0} {

			    if {[catch {WPCmd PEMessage $uid fromaddr} fromaddr]} {
			      set fromaddr ""
			    } elseif {$showimg == 0} {
			      if {[catch {WPSessionState allow_cid_images} friends] == 0} {
				while {[set findex [lsearch -exact $friends $fromaddr]] >= 0} {
				  set friends [lreplace $friends $findex $findex]
				  if {[catch {WPSessionState allow_cid_images $friends} friends]} {
				    catch {WPCmd PEInfo statmsg "Cannot forget image sender: $friends"}
				    break;
				  }
				}
			      }

			      set showimg {}
			    }

			    if {[string length $showimg]
				|| ([catch {WPSessionState allow_cid_images} friends] == 0
				    && [lsearch -exact $friends $fromaddr] >= 0)} {

			      set showimages 1

			      if {[string length $fromaddr]} {
				set bodyleadin "\[ Attached images ARE being displayed \]"
				if {$showimg == 1} {
				  append bodyleadin "[cgi_nl]\[ Always show images from [cgi_url "$fromaddr" "wp.tcl?page=body&showimg=[WPPercentQuote $fromaddr]"] \]"
				} else {
				  append bodyleadin "[cgi_nl]\[ Never show images from [cgi_url "$fromaddr" "wp.tcl?page=body&showimg=0"] \]"

				  if {[catch {WPSessionState allow_cid_images} friends] || [llength $friends] == 0} {
				    set friends $fromaddr
				  } elseif {[lsearch -exact $friends $fromaddr] < 0} {
				    lappend friends $fromaddr
				  }

				  if {[catch {WPSessionState allow_cid_images $friends} friends]} {
				    catch {WPCmd PEInfo statmsg "Cannot remember image sender: $friends"}
				  }
				}
			      }
			    } else {

			      set showimages 0

			      set bodyleadin "\[ Attached images are NOT being displayed \]"
			      append bodyleadin "[cgi_nl]\[ Show images [cgi_url "below" "wp.tcl?page=body&showimg=1"]"
			      if {[string length $fromaddr]} {
				append bodyleadin ", or always show images from [cgi_url "$fromaddr" "wp.tcl?page=body&showimg=[WPPercentQuote $fromaddr]"] \]"
			      } else {
				append bodyleadin " \]"
			      }

			    }
			  }
			}
		      }
		      default {}
		    }
		  }
		}

		set inbody 0
		foreach i $msgtext {

		  if {!$inbody
		      && [llength $i] == 1
		      && [lindex [lindex $i 0] 0] == "t"
		      && 0 == [string length [lindex [lindex $i 0] 1]]} {
		    set inbody 1
		    if {[info exists bodyleadin]} {
		      cgi_puts $bodyleadin
		    }
		  }

		  foreach j $i {
		    set ttype [lindex $j 0]
		    set tdata [lindex $j 1]

		    # write anchors by hand
		    switch -- $ttype {
		      urlstart {
			set href [lindex $tdata 0]
			set name [lindex $tdata 1]

			# build links by hand since we don't know where
			# they'll terminate
			set linktext "<a "
			switch -- [lindex [split $href :] 0] {
			  XXX_mailto {
			    append linktext "href=\"null.tcl\" onClick=\"composeMsg('mailto','[lindex [split $href :] 1]','$webpine_key'); return false;\""
			  }
			  default {
			    if {[regexp -- "^\[a-zA-Z\]+:" $href ]} {
			      append linktext "href=\"$href\" target=_blank"
			    } ;# no relative links for security

			      if {[string length $name]} {
				append linktext "name=\"$name\""
			      }
			    }
			  }

			  cgi_put "${linktext}>"
			  set inurl 1
			}
			urlend {
			  if {$inurl} {
			    cgi_put "</a>"
			  }
			}
			attach {
			  set attachuid [lindex $tdata 0]
			  set part [lindex $tdata 1]
			  set mimetype [lindex $tdata 2]
			  set mimesubtype [lindex $tdata 3]
			  if {[string length [lindex $tdata 4]]} {
			    set file [lindex $tdata 4]
			  } else {
			    if {[string length [lindex $tdata 5]]} {
			      set file "attachment.[lindex $tdata 5]"
			    } else {
			      set file "unknown.txt"
			    }
			  }

			  set attachurl "detach.tcl?uid=${attachuid}&part=${part}"
			  set saveurl "${attachurl}&download=1"
			  if {0 == [string compare -nocase $mimetype "text"]
			      && 0 == [string compare -nocase $mimesubtype "html"]} {
			    append attachurl "&download=1"
			  }

			  set attachexp "View ${mimetype}/${mimesubtype} Attachment"

			  if {0 == [string compare message [string tolower ${mimetype}]]
			      && 0 == [string compare rfc822 [string tolower ${mimesubtype}]]} {
			    set attmsgurl "wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key]&uid=${attachuid}&part=${part}&style="
			    cgi_put [cgi_url [cgi_font size=-1 Fwd] "${attmsgurl}Forward" target=_top]
			    cgi_put "|[cgi_url [cgi_font size=-1 Reply] "${attmsgurl}Reply&reptext=1&repall=1&repqstr=[WPPercentQuote [WPCmd PEMessage $uid replyquote]]" target=_top]"

			  } else {
			    cgi_put [cgi_url [cgi_font size=-1 View] $attachurl target=_blank]
			    cgi_put "|[cgi_url [cgi_font size=-1 Save] $saveurl]"
			  }

			  if {[info exists attachurl]} {
			    set attachtext [WPurl $attachurl {} $hacktoken $attachexp target=_blank]
			  }
			}
			img {
			  if {[info exists showimages] && $showimages == 1
			      && [catch {WPCmd PEMessage $uid cid "<[lindex [lindex $j 1] 0]>"} cidpart] == 0
			      && [string length $cidpart]
			      && [catch {WPCmd PEMessage $uid attachinfo $cidpart} attachinfo] == 0
			      && [string compare [string tolower [lindex $attachinfo 1]] image] == 0} {
			    cgi_put [cgi_img detach.tcl?uid=${uid}&part=${cidpart} "alt=\[[lindex $tdata 1]\]"]
			  } else {
			    cgi_put "\[[lindex $tdata 1]\]"
			  }
			}
			fgcolor {
			  if {$infont} {
			    cgi_put "</font>"
			    set infont 0
			  }

			  if {[string compare $tdata $fgcolor]} {
			    set fgcolor $tdata
			    if {[string compare $fgcolor $normal_fgcolor]} {
			      cgi_put "<font color=#${tdata}>"
			      set infont 1
			    }
			  }
			}
			bgcolor {
			  if {$infont} {
			    cgi_put "</font>"
			    set infont 0
			  }

			  if {[string compare $tdata $bgcolor]} {
			    cgi_put "<font style=\"background: #${tdata}\">"
			    set bgcolor $tdata
			    set infont 1
			  }
			}
			color {
			  if {$infont} {
			    cgi_put "</font>"
			    set infont 0
			  }

			  if {[string compare [lindex $tdata 0] $fgcolor] || [string compare [lindex $tdata 1] $bgcolor]} {
			    cgi_put "<font color=#[lindex $tdata 0] style=\"background: #[lindex $tdata 1]\">"
			    set bgcolor $tdata
			    set infont 1
			  }
			}
			italic {
			  switch $tdata {
			    on { cgi_put "<i>" }
			    off { cgi_put "</i>" }
			  }
			}
			bold {
			  switch $tdata {
			    on { cgi_put "<b>" }
			    off { cgi_put "</b>" }
			  }
			}
			underline {
			  switch $tdata {
			    on { cgi_put "<u>" }
			    off { cgi_put "</u>" }
			  }
			}
			strikethru {
			  switch $tdata {
			    on { cgi_put "<s>" }
			    off { cgi_put "</s>" }
			  }
			}
			bigfont {
			  switch $tdata {
			    on { cgi_put "<font size=\"+1\">" }
			    off { cgi_put "</font>" }
			  }
			}
			smallfont {
			  switch $tdata {
			    on { cgi_put "<font size=\"-1\">" }
			    off { cgi_put "</font>" }
			  }
			}
			default {
			  if {[info exists attachtext] && [set ht [string first $hacktoken $attachtext]] >= 0} {
			    set firstbit [string range $attachtext 0 [expr {$ht - 1}]]
			    set lastbit [string range $attachtext [expr {$ht + [string length $hacktoken]}] end]
			    cgi_put "    $firstbit[cgi_quote_html [string trimleft $tdata]]$lastbit"
			    unset attachtext
			  } else {
			    cgi_put [cgi_quote_html $tdata]
			  }
			}
		      }
		    }

		    cgi_puts ""
		  }
		}
	      }
	    }

	    if {![info exists attachuid]} {
	      if {!([catch {WPCmd PEMessage $uid attachinfo 1} typing]
	      || [string compare [string tolower [lindex $typing 1]] text]
	      || [string compare [string tolower [lindex $typing 2]] html])} {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts [cgi_font font-family=fixed "\[Note: you may also [cgi_url view "detach.tcl?uid=${uid}&part=1" target=_blank] HTML message directly in your browser\]"]
		  }
		}
	      }
	    }
	  }

	  if {$infont} {
	    cgi_put "</font>"
	  }

	  if {$inurl} {
	    cgi_put "</a>"
	  }

	  if {[info exists _wp(cumulative)]} {
	    set l [string length $_wp(cumulative)]
	    if {$l < 6} {
	      set sl "."
	      while {$l < 6} {
		append sl "0"
		incr l
	      }
	      append sl $_wp(cumulative)
	    } else {
	      set sl "[string range $_wp(cumulative) 0 [expr $l - 7]].[string range $_wp(cumulative) [expr $l - 6] end]"
	    }

	    set servlettime "servlet = $sl"

	    if {[info exists wp_global_loadtime]} {
	      set clickdiff [expr {[clock clicks] - $wp_global_loadtime}]
	      # 500165 clicks/second
	      set st [expr ([string range $clickdiff 0 [expr [string length $clickdiff] - 4]] * 1000) / 500]
	      set l [string length $st]
	      set scripttime "tcl = [string range $st 0 [expr $l - 4]].[string range $st [expr $l - 3] end], "
	    } else {
	      set scripttime ""
	    }

	    cgi_puts [cgi_font size=-2 "style=font-family:sans-serif;font-weight:bold" "\[time: ${scripttime}${servlettime}\]"]
	  }
	}
      }
    }
  }
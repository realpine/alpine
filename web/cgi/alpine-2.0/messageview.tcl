# Web Alpine message text painting support
# $Id$
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

#  messageview
#
#  Purpose:  TCL procedure to produce HTML-based view of a message's
#            header/body text
#
#  Input:
#        

proc email_addr {pers mailbox} {
  if {[string length $pers]} {
    return "[cgi_quote_html $pers] [cgi_lt][cgi_quote_html $mailbox][cgi_gt]"
  } else {
    return "[cgi_quote_html $mailbox]"
  }
}

proc put_quoted {s} {
  cgi_put [cgi_quote_html $s]
}

proc put_quoted_nl {s} {
  cgi_put "[cgi_quote_html $s][cgi_nl]"
}

proc navViewButtons {c f u n} {
  cgi_table_data  class="wap pageBtns" {
    if {[catch {PEMailbox uid [PEMailbox next $n -1]} uprev]} {
      set prevurl "#"
    } elseif {$u == $uprev} {
      set prevurl "javascript:alert('Already viewing first message')"
    } else {
      set prevurl "view/$c/$f/$uprev"
    }

    cgi_put [cgi_url [cgi_span "class=sp spmb spmbu" [cgi_span "Prev"]] $prevurl "title=Previous Message" "onClick=return newMessageText({control:this,parms:{op:'prev'}});"]
  }

  cgi_table_data class="wap dv1" {
    cgi_puts [cgi_span "class=sp spmb spmb3" ""]
  }

  cgi_table_data class="wap pageBtns" {
    if {[catch {PEMailbox uid [PEMailbox next $n 1]} unext]} {
      set nexturl "#"
    } elseif {$u == $unext} {
      set nexturl "javascript:alert('Already viewing last message')"
    } else {
      set nexturl "view/$c/$f/$unext"
    }

    cgi_put [cgi_url [cgi_span "class=sp spmb spmbd" [cgi_span "Next"]] $nexturl title="Next Message" class=wap "onClick=return newMessageText({control:this,parms:{op:'next'}});"]
  }
}

proc drawTopViewMenuBar {c f u n} {
  cgi_table id=toolBar class="toolbarTbl" cellpadding="0" cellspacing="0" {
    cgi_puts "<tbody>"
    cgi_table_row  {
      if {[lsearch -exact [list Junk Trash] $f] >= 0} {
	cgi_table_data  class=wap {
	  cgi_puts [cgi_url "[cgi_img "img/cbn/delete.gif" class=wap] Delete Forever" "empty/$c/$f/$u" "onClick=return doEmpty(this);" "title=Move message to Trash" class=wap]
	}
	cgi_table_data  class="wap dv1" {
	  cgi_puts [cgi_span "class=sp spmb spmb3" ""]
	}
	cgi_table_data  class=wap {
	  cgi_puts [cgi_url "[cgi_img img/cbn/inbox.gif class=wap] Move to INBOX" "move/$c/$f/$u?df=0/INBOX" "onClick=return newMessageText({control:this,parms:{op:'move',df:'0/INBOX'}});"  class=wap]
	}
      } else {
	cgi_table_data class=wap {
	  cgi_put [cgi_url [cgi_span "class=sp spmbi spmb4" "Reply"] "reply/$c/$f/$u?pop=view/$c/$f/$u" id=gReply class=wap]
	}
	cgi_table_data class=wap {
	  cgi_put [cgi_url [cgi_span "class=sp spmbi spmb5" "Reply All"] "replyall/$c/$f/$u?pop=view/$c/$f/$u" id=gReplyAll class=wap]
	}
	cgi_table_data class=wap {
	  cgi_put [cgi_url [cgi_span "class=sp spmbi spmb6" "Forward"] "forward/$c/$f/$u?pop=view/$c/$f/$u" id=gForward class=wap]
	}
	cgi_table_data class=wap {
	  if {[catch {PEMailbox uid [PEMailbox next $n 1]} unext]} {
	    set delurl "browse/$c/$f?u=$u"
	  } elseif {$u == $unext} {
	    set delurl "browse/$c/$f?u=${u}&delete=$u"
	  } else {
	    set delurl "view/$c/$f/$unext?delete=$u"
	  }
	  cgi_put [cgi_url [cgi_span "class=sp spmbi spmb7" "Delete"] $delurl title="Move message to Trash" class=wap id=gDelete "onClick=return newMessageText({control:this,parms:{op:'delete'}});"]
	}
	cgi_table_data class=wap {
	  cgi_put [cgi_url [cgi_span "class=sp spmbi spmb8" "Report Spam"] "view/$c/$f/$unext?spam=$u" title="Report as Spam and move message to Junk folder" class=wap id=gSpam "onClick=return newMessageText({control:this,parms:{op:'spam'}});"]
	}
	cgi_table_data  class="wap dv1" {
	  cgi_puts [cgi_span "class=sp spmb spmb3" ""]
	  #cgi_put [cgi_img "img/cbn/div.gif" class=wap]
	}
	cgi_table_data class=wap {
	  cgi_bullet_list class="menu" {
	    cgi_put "<li class=menuHdr>[cgi_url "More Actions [cgi_img "img/cbn/menu.gif" class="wap menuDn menuImg"]" "#" class=wap "onClick=return false;"]<div>"
	    cgi_bullet_list {
	      cgi_li [cgi_url "Extract Addresses (Take)" "extract/$c/$f/$u" id="gExtract" "onClick=return takeAddress();"]
	      cgi_li "<hr />[cgi_url "Mark as Unread" "view/$c/$f/$unext?unread=$u" "onClick=return newMessageText({control:this,parms:{op:'unread'}});" id=gUnread]"
	      cgi_li [cgi_url "Set Star" "view/$c/$f/$u?star=1" "onClick=return setStar(-1,'ton');"]
	      cgi_li [cgi_url "Clear Star" "view/$c/$f/$u?star=0" "onClick=return setStar(-1,'not');"]
	    }
	    cgi_put "</div></li>"
	  }
	}
      }

      cgi_table_data  class="wap dv1" {
	cgi_puts [cgi_span "class=sp spmb spmb3" ""]
      }

      # move/cp FOLDER LIST
      cgi_table_data id=viewMorcButton class="wap yui-skin-sam yuimenu" {}
      cgi_table_data class=wap {
	cgi_bullet_list class="menu" {
	  cgi_put "<li class=menuHdr>[cgi_url "to Folder [cgi_img "img/cbn/menu.gif" class="menuDn menuImg wap"]" "#" "onClick=return false;"]<div>"
	  cgi_bullet_list id=viewSaveCache {
	    cgi_li "<hr />[cgi_url "More Folders..." "save/$c/$f" "onClick=pickFolder('folderList',morcWhich('viewMorcButton'),'[WPCmd PEFolder defaultcollection]',morcInViewDone); return false;"]"
	  }
	  cgi_put "</div></li>"
	}
      }

      cgi_table_data class=wap width="100%" {
	cgi_put [cgi_nbspace]
      }

      cgi_puts [navViewButtons $c $f $u $n]

      cgi_table_data  class="wap tbPad" align="right" {
	cgi_put [cgi_nbspace]
      }
    }
    cgi_puts "</tbody>"
  }
}

proc drawBottomViewMenuBar {c f u n mc} {
  cgi_table class="wap toolbarTbl" cellpadding="0" cellspacing="0" {
    cgi_puts "<tbody>"
    cgi_table_row  {
      cgi_table_data class="wap pageText" id=viewContext {
	cgi_put "Message $n of $mc"
      }
      cgi_table_data class=wap width="100%" {
	cgi_put [cgi_nbspace]
      }

      cgi_puts [navViewButtons $c $f $u $n]

      cgi_table_data  class="wap tbPad" align="right" {
	cgi_put [cgi_nbspace]
      }
    }
    cgi_puts "</tbody>"
  }
}

proc drawMessageText {c f u {showimg ""}} {
    # before getting the list, move anything deleted to Trash
    if {[catch {PEMailbox trashdeleted current} result]} {
      PEInfo statmsg "Trash move Failed: $result"
    }

    # remember "current" message
    if {$u > 0 && [catch {PEMailbox current uid $u} cm]} {
      PEInfo statmsg "Set current failed: $cm" 
      set u 0
    }

    set wasnew [expr {[lsearch -exact [PEMessage $u status] New] >= 0}]
    set html 0

    catch {unset shownsubtype}
    catch {unset bodyleadin}
    catch {unset fromaddr}

    # collect message parts
    if {[catch {PEMessage $u header} hdrtext]} {
      set hdrtext "Message Text Fetch Failure: $hdrtext"
    } else {
      # fish out From: address
      foreach {ht hf hd} [join $hdrtext] {
	if {0 == [string compare addr $ht]} {
	  if {0 == [string compare -nocase from $hf]} {
	    if {[llength $hd] == 1} {
	      set df [lindex [lindex $hd 0] 0]
	      if {0 == [string length $df]} {
		set df [lindex [lindex $hd 0] 1]
	      }

	      set fromaddr [list $df [cgi_quote_html [lindex [lindex $hd 0] 0]] [lindex [lindex $hd 0] 1]]
	    }

	    break
	  }
	}
      }
    }

    set attachments 0
    if {[catch {PEMessage $u attachments} attachmentlist]} {
      # STATUS "Cannot get attachments: $attachments"
    } else {
      foreach att $attachmentlist {
	# only an attachment if "shown" in multi/alt AND it has a filename
	if {[string compare [lindex $att 1] shown]} {
	  if {[string length [lindex $att 4]]} {
	    incr attachments
	  }
	} else {
	  # only use subtype if a single part is intended to be displayed
	  if {[info exists shownsubtype]} {
	    set shownsubtype ""
	  } else {
	    set shownsubtype [string tolower [lindex $att 3]]
	  }
	}
      }
    }

    set bodyopts {}
    set showimages ""

    if {[info exists shownsubtype] && 0 == [string compare $shownsubtype html]} {
      if {[WPCmd PEInfo feature render-html-internally] == 0} {
	set html 1
	lappend bodyopts html
	set image_set allow_images
      } else {
	set image_set allow_cid_images
      }

      switch -- $showimg {
	1 {			;# show images this once
	  lappend bodyopts images
	  set showimages 1
	}
	from {			;# always show images from sender
	  lappend bodyopts images
	  set showimages friend
	  if {[info exists fromaddr] && [string length [lindex $fromaddr 2]]} {
	    if {[catch {WPSessionState $image_set} friends]} {	;# no image_set yet?
	      if {[catch {WPSessionState $image_set [list [lindex $fromaddr 2]]} result]} {
		catch {PEInfo statmsg "Cannot remember [lindex $fromaddr 2]: $result"}
	      }
	    } elseif {[lsearch -exact $friends [lindex $fromaddr 2]] < 0} {
	      lappend friends [lindex $fromaddr 2]
	      if {[catch {WPSessionState $image_set $friends} result]} {
		catch {PEInfo statmsg "Cannot remember [lindex $fromaddr 2]: $result"}
	      }
	    }
	  }
	}
	0 {			;# never show images from sender
	  if {[info exists fromaddr] && [catch {WPSessionState $image_set} friends] == 0} {
	    while {[set findex [lsearch -exact $friends [lindex $fromaddr 2]]] >= 0} {
	      set friends [lreplace $friends $findex $findex]
	      if {[catch {WPSessionState $image_set $friends} friends]} {
		catch {PEInfo statmsg "Cannot forget image sender: $friends"}
		break;
	      }
	    }
	  }
	}
	"" {			;# display if a friend
	  if {[info exists fromaddr]
	      && 0 == [catch {WPSessionState $image_set} friends]
	      && [lsearch -exact $friends [lindex $fromaddr 2]] >= 0} {
	    lappend bodyopts images
	    set showimages friend
	  }
	}
      }
    }

    if {[PEInfo mode full-header-mode] > 0} {
      set put_tdata put_quoted
    }

    if {[catch {PEMessage $u body $bodyopts} bodytext]} {
      set bodytext "Message Text Fetch Failure: $bodytext"
    }

    # prescan body for interesting stuff
    foreach i $bodytext {
      foreach j $i {
	switch -exact [lindex $j 0] {
	  img {			;# digested HTML mode image
	    if {![info exists bodyleadin]
		&& [catch {PEMessage $uid cid "<[lindex [lindex $j 1] 0]>"} cidpart] == 0
		&& [string length $cidpart]
		&& [catch {PEMessage $uid attachinfo $cidpart} attachinfo] == 0
		&& [string compare [string tolower [lindex $attachinfo 1]] image] == 0} {
	      if {[string length $showimages]} {
		set bodyleadin "\[ Attached images ARE being displayed \]"
		if {[info exists fromaddr] && [string length [lindex $fromaddr 2]]} {
		  if {[string compare $showimages friend]} {
		    append bodyleadin "[cgi_nl]\[ Never show images from [cgi_url "[lindex $fromaddr 0]" "view/$c/$f/$u?showimg=0"] \]"
		  } else {
		    append bodyleadin "[cgi_nl]\[ Always show images from [cgi_url "[lindex $fromaddr 0]" "view/$c/$f/$u?showimg=from"] \]"
		  }
		}
	      } else {
		set bodyleadin "\[ Attached images are NOT being displayed \]"
		append bodyleadin "[cgi_nl]\[ Show images [cgi_url "below" "view/$c/$f/$u?showimg=1" "onClick=return newMessageText({control:this,parms:{op:'noop',img:'1'}});"]"
		if {[info exists fromaddr] && [string length [lindex $fromaddr 2]]} {
		  append bodyleadin ", or always show images from [cgi_url "[lindex $fromaddr 0]" "from/$c/$f/$u?showimg=from"] \]"
		} else {
		  append bodyleadin " \]"
		}
	      }
	    }
	  }
	  t {
	    if {$html && ![info exists bodyleadin] && [regexp {<[Ii][Mm][Gg](| ([^>]+))>} [lindex $j 1] dummy img]} {
	      set bodyleadin "[cgi_img "img/cbn/infomsg.gif"] Web-based images "
	      if {![string length $showimages]} {
		append bodyleadin "have been blocked to to help protect your privacy and reduce spam.[cgi_nl][cgi_url "Display images below" "view/$c/$f/$u?showimg=1" "onClick=return newMessageText({control:this,parms:{op:'noop',img:'1'}});"]"
		if {[info exists fromaddr] && [string length [lindex $fromaddr 2]]} {
		  append bodyleadin "[cgi_nbspace][cgi_nbspace][cgi_nbspace]-[cgi_nbspace][cgi_nbspace][cgi_nbspace][cgi_url "Display images and always trust email from [lindex $fromaddr 0]" "view/$c/$f/$u?showimg=from" "onClick=return newMessageText({control:this,parms:{op:'noop',img:'from'}});"]"
		}
	      } else {
		append bodyleadin "are being displayed below.[cgi_nl]"
		if {[info exists fromaddr]} {
		  if {[string compare friend $showimages]} {
		    append bodyleadin "[cgi_nbspace][cgi_nbspace][cgi_url "Always display images and trust email from [lindex $fromaddr 0]" "view/$c/$f/$u?showimg=from" "onClick=return newMessageText({control:this,parms:{op:'noop',img:'from'}});"]"
		  } else {
		    append bodyleadin "[cgi_url "No longer automatically display images from [lindex $fromaddr 0]" "view/$c/$f/$u?showimg=0" "onClick=return newMessageText({control:this,parms:{op:'noop',img:'0'}});"]"
		  }
		}
	      }
	    }
	  }
	  default {}
	}
      }
    }

    set infont 0
    set inurl 0
    set fgcolor [set normal_fgcolor [PEInfo foreground]]
    set bgcolor [PEInfo background]

    # Toss any informational panels up here
    if {[info exists bodyleadin]} {
      cgi_division  class="wap bannerPrivacy" {
	cgi_puts $bodyleadin
      }
    }

    # put message pieces together
    # first the header & attachment list
    cgi_table class="wap msgHead" cellpadding="0" cellspacing="0" {
      cgi_puts "<tbody>"

      # user headers are selectable, but we control the
      # first two: subject, from and
      # last two: date, attachments
      if {1 == [llength $hdrtext] && 0 == [string compare -nocase raw [lindex [lindex $hdrtext 0] 0]]} {
	# raw header
	cgi_table_row {
	  cgi_table_data {
	    cgi_preformatted {
	      foreach hdrline [lindex [lindex $hdrtext 0] 2] {
		foreach hdrlinepart $hdrline {
		  switch -- [lindex $hdrlinepart 0] {
		    t {
		      cgi_put [cgi_quote_html [lindex $hdrlinepart 1]]
		    }
		    default {
		    }
		  }
		}

		cgi_br
	      }
	    }

	    cgi_table_data "class=\"wap fullHdrBtn\"" {
	      if {[WPCmd PEMailbox focus]} {
		set retf "Search Result"
	      } else {
		set retf [cgi_quote_html $f]
	      }

	      cgi_put [cgi_url "Return[cgi_nbspace]to[cgi_nbspace]${retf}" "browse/$c/$f" class="wap" "onClick=return newMessageList({contol:this});"]
	      cgi_br
	      cgi_put [cgi_url "Show[cgi_nbspace]Filtered[cgi_nbspace]Headers" "view/$c/$f/$u" class="wap" "onClick=return newMessageText({contol:this,parms:{op:'hdroff'}});"]
	    }
	  }
	}
      } else {
	set hdrmarkup ""
	set datemarkup ""
	foreach {ht hf hd} [join $hdrtext] {
	  switch -exact $ht {
	    text {
	      if {0 == [string compare -nocase subject $hf]} {
		cgi_table_row {
		  cgi_table_data colspan=2 class="subText" {
		    cgi_put [cgi_quote_html $hd]
		  }
		  cgi_table_data "class=\"wap fullHdrBtn\"" {
		    if {[WPCmd PEMailbox focus]} {
		      set retf "Search Result"
		    } else {
		      set retf [cgi_quote_html $f]
		    }

		    cgi_put [cgi_url "Return[cgi_nbspace]to[cgi_nbspace]${retf}[cgi_span "class=sp spmv spmv3" [cgi_span [cgi_nbspace]]]" "browse/$c/$f" class="wap" "onClick=return newMessageList({contol:this,parms:{page:'new'}});"]
		  }
		}
	      } elseif {0 == [string compare -nocase date $hf]} {
		set datemarkup "<td class=\"wap hdrLabel\">$hf:</td><td class=\"wap hdrText\">[cgi_quote_html $hd]</td>"
	      } else {
		append hdrmarkup "<td class=\"wap hdrLabel\">$hf:</td><td colspan=2 class=\"wap hdrText\">[cgi_quote_html $hd]</td>"
	      }
	    }
	    addr {
	      if {0 == [string compare -nocase from $hf]} {	;# From: always first
		set addrs ""
		if {0 == [catch {PEAddress books} booklist]} {
		  set tAFargs "\{books:\["
		  set comma ""
		  foreach b $booklist {
		    regsub -all {'} [lindex $b 1] {\'} bname
		    append tAFargs "${comma}\{book:[lindex $b 0],name:'$bname'\}"
		    set comma ","
		  }

		  append tAFargs "\]\}"
		} else {
		  set tAFargs {{}}
		}

		foreach ma $hd {
		  lappend addrs [cgi_span class=emailAddr "[cgi_span "class=contactAddr" [email_addr [lindex $ma 0] [lindex $ma 1]]] [cgi_url "[cgi_span "class=sp spmv spmv1" ""]Add" "" "class=wap addContact" "onClick=takeAddressFrom('$u',$tAFargs); return false;" "title=Add Sender to Contacts"]"]
		}

		set hdrmarkup [linsert $hdrmarkup 0 "<td class=\"wap hdrLabel\">$hf:</td><td colspan=2 class=\"wap hdrText\">[join $addrs ",[cgi_nl]"]</td>"]
	      } else {
		set addrs ""
		foreach ma $hd {
		  lappend addrs [cgi_span class=emailAddr [email_addr [lindex $ma 0] [lindex $ma 1]]]
		}

		lappend hdrmarkup "<td class=\"wap hdrLabel\">$hf:</td><td colspan=2 class=\"wap hdrText\">[join $addrs ", "]</td>"
	      }
	    }
	    news -
	    rawaddr {
	      lappend hdrmarkup "<td class=\"wap hdrLabel\">$hf:</td><td colspan=2 class=\"wap hdrText\">[cgi_quote_html [join $hd ", "]]</td>"
	    }
	    default {
	    }
	  }
	}

	foreach hm $hdrmarkup {
	  cgi_table_row {
	    cgi_puts $hm
	  }
	}

	cgi_table_row {
	  cgi_puts $datemarkup
	  cgi_table_data "class=\"wap fullHdrBtn\"" {
	    cgi_put [cgi_url "Show[cgi_nbspace]Full[cgi_nbspace]Headers" "view/$c/$f/$u?headers=full" class="wap" "onClick=return newMessageText({control:this,parms:{op:'hdron'}});"]
	  }
	}
      }

      if {$attachments} {
	cgi_table_row {
	  cgi_table_data class="wap hdrLabel" {
	    cgi_put "Attachments:"
	  }
	  cgi_table_data class="wap hdrText" {
	    cgi_division "class=attach" {
	      set tail ""
	      foreach att $attachmentlist {
		# !shown and has supplied file name
		if {[string compare [lindex $att 1] shown] && [string length [lindex $att 4]]} {
		  if {[string length [lindex $att 4]]} {
		    set attitle [lindex $att 4]
		  } else {
		    set attitle "Attachment [lindex $att 0]"
		  }

		  cgi_put [cgi_span "class=attach" "[cgi_span "class=sp spmv spmv2" ""][cgi_url "$attitle" "detach/$c/$f/$u/[lindex $att 0]?download=1" title="Download $attitle"]"]
		}

		set tail ":[cgi_nbspace]"
	      }
	    }
	  }
	}
      }
      cgi_table_row {
	cgi_table_data colspan="3" class="hdrLabel" {
	  cgi_put [cgi_span "class=trans" "style=height:5px;width:2px;" [cgi_span " "]]
	}
      }

      cgi_puts "</tbody>"
    }

    # start writing body
    if {$html} {
      set class ""
      if {![info exists put_tdata]} {
	set put_tdata cgi_put
      }
    } else {
      set class "class=\"contentBody messageText\""
      if {![info exists put_tdata]} {
	set put_tdata put_quoted
      }
    }

    cgi_division $class {
      foreach i $bodytext {
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
		mailto {
		  regsub {[?]} [lindex [split $href :] 1] {\&} maito
		  append linktext "href=mailto?to=[WPPercentQuote $maito {&=}]&pop=view/$c/[WPPercentQuote $f {/}]/$u"
		}
		default {
		  # no relative links, no javascript
		  if {[regexp -- "^(\[a-zA-Z\]+):" $href dummy scheme]
		      && [string compare -nocase $scheme javascript]} {
		    append linktext "href=\"$href\" target=\"_blank\" "
		  } 

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
		set attachurl [append $attachurl "&download=1"]
	      }

	      set attachexp "View ${mimetype}/${mimesubtype} Attachment"

	      if {0 == [string compare message [string tolower ${mimetype}]]
		  && 0 == [string compare rfc822 [string tolower ${mimesubtype}]]} {
		set attmsgurl "/$c/$f/$u/$part"
		cgi_put [cgi_url [cgi_font size=-1 Fwd] "forward${attmsgurl}" target=_top]
		cgi_put "|[cgi_url [cgi_font size=-1 Reply] "reply${attmsgurl}?reptext=1&repall=1" target=_top]"

	      } else {
		cgi_put [cgi_url [cgi_font size=-1 View] $attachurl target="_blank"]
		cgi_put "|[cgi_url [cgi_font size=-1 Save] $saveurl]"
	      }

	      if {[info exists attachurl]} {
		set attachtext [WPurl $attachurl {} $hacktoken $attachexp target="_blank"]
	      }
	    }
	    img {
	      if {[info exists showimages] && [string length $showimages]
		  && [catch {PEMessage $uid cid "<[lindex [lindex $j 1] 0]>"} cidpart] == 0
		  && [string length $cidpart]
		  && [catch {PEMessage $uid attachinfo $cidpart} attachinfo] == 0
		  && [string compare [string tolower [lindex $attachinfo 1]] image] == 0} {
		cgi_put [cgi_img detach/$c/$f/$u/${cidpart} "alt=\[[lindex $tdata 1]\]"]
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
		$put_tdata $tdata
	      }
	    }
	  }
	}

	if {!$html} {
	  cgi_br
	}
      }
    }

    cgi_division id="skip" {
      cgi_put [cgi_url "Skip to Toolbar" "#toolbar"]
    }

    if {$wasnew} {
      cgi_put "<script>decUnreadCount(1);</script>"
    }
}

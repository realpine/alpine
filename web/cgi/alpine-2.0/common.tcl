# $Id: common 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  common.tcl
#
#  Purpose:  TCL script snippets that the various CGI script generating pages
#            have in common
#

proc wpSelectedClass {seld unread defclass} {
  set class $defclass
  if {[expr $seld]} {
    append class " sel"
  }

  if {$unread} {
    append class " bld"
  }

  return $class
}

proc wpPageTitle {page} {
  return "$page - Web Alpine 2.0"
}

proc current_context {page c f cc cf} {
  set ct {[lsearch {browse view} $page] >= 0 && }
  if {0 == $cc && 0 == [string compare -nocase inbox $cf]} {
    append ct {0 == $c && 0 == [string compare -nocase inbox $f]}
  } else {
    append ct {$cc == $c && 0 == [string compare $cf $f]}
  }

  set current [expr $ct]
}

proc folder_link {current page c f u unread {ficon ""}} {
  set url "browse/$c/[WPPercentQuote $f]"
  set clickurl $url
  set urlid ""
  set fclass ""
  set fid ""
  if {$current} {
    set urlid "id=\"gFolder\""
    set urid "id=\"unreadCurrent\""
    set fid "id=\"fCurrent\""
    set onclick "onClick=return newMessageList({parms:{op:'unfocus',page:'new'}});"
  } elseif {[string length $ficon]} {
    set fq [cgi_quote_html $f]
    set urid "id=\"unread${fq}\""
    set fid "id=\"f${fq}\""
    set onclick [onClick $clickurl]
  } else {
    set urid ""
    set onclick ""
  }

  if {$unread} {
    set fclass "class=bld"
    set urc " ($unread)"
  } elseif {[string length $ficon] || $current} {
    set urc ""
  }

  if {[info exists urc]} {
    set urt [cgi_span "class=wap unrd" $urid $urc]
  } else {
    set urt ""
  }

  if {![string length $ficon]} {
    set ficon [cgi_span "class=sp splc splc7" ""]
  }
  return "[cgi_url "$ficon[cgi_span $fid $fclass [cgi_quote_html $f]]" $url class=wap $urlid $onclick]$urt"
}

proc empty_link {current falias c} {
  if {$current} {
    set emptyfunc emptyCurrent
  } else {
    switch $falias {
      Trash { set emptyfunc emptyTrash}
      Junk { set emptyfunc emptyJunk }
    }
  }
  if {[info exists emptyfunc]} {
    return "\[[cgi_url "Empty" "#" "onClick=panelConfirm('Are you sure you want to permanently delete the contents of the $falias folder?<p>Deleted messages are gone forever.',{text:'Empty $falias',fn:$emptyfunc}); return false;" title="Permanently delete all messages in the $falias folder" class=wap]\]"
  }

  return ""
}

proc context_class {current} {
  if {$current && ![WPCmd PEMailbox focus]} {
    return "fld sel"
  }

  return "fld"
}

proc div_folder {page c f u cc cf unread {ficon ""}} {
  set current [current_context $page $c $f $cc $cf]
  cgi_division class="[context_class $current]" {
      cgi_put [folder_link $current $page $cc $cf $u $unread $ficon]
  }
}

proc wpStatusAndNewmailJavascript {} {
  foreach sm [WPCmd PEInfo statmsgs] {
    regsub -all {'} $sm {\'} sm
    cgi_puts "  addStatusMessage('$sm',15);"
  }
  if {[catch {WPCmd PEMailbox newmail 0} newmail]} {
    regsub -all {'} $newmail {\'} newmail
    cgi_puts "  addStatusMessage('new mail error: $newmail',15);"
  } else {
    catch {WPCmd PEMailbox newmailreset}
    if {[lindex $newmail 0] > 0} {
      cgi_puts "  addStatusMessage(parseNewMail('$newmail'),15);"
    }
  }
  cgi_puts "  displayStatusMessages();"
}

set clicktest ""

proc onClick {dest} {
  global clicktest

  if {[string length $clicktest]} {
    return "onClick=[$clicktest $dest]"
  }

  return ""
}

# table representing common overall page layout
#
# NOTES: bodyform doesn't flow thru cgi.tcl,but it keeps menu bar's
#        easier for caller to keep stright
#
proc wpCommonPageLayout {curpage c f u context searchform leavetest cmds menubar_top content menubar_bottom} {
  global _wp clicktest

  set thispage [lindex $curpage 0]

  set clicktest $leavetest

  # various positioned elements

  # busy cue
  cgi_division id="bePatient" {
    cgi_put "Working..."
  }

  cgi_table class="page" cellpadding="0" cellspacing="0" {
    cgi_puts "<tbody>"

    cgi_table_row  {
      cgi_table_data  class="spc" colspan="3" {
	cgi_put [cgi_span "class=sp trans" "style=height:1px;width:2px;" [cgi_span " "]]
      }
    }

    # CONTEXT
    cgi_table_row {
      cgi_table_data class="wap topHdr" {
	cgi_division id=hdrLogo {
	  cgi_puts [cgi_img "img/cbn/alpinelogo.gif" name="logo" class="logo" "title=Web Alpine [WPCmd PEInfo version]" class=wap]
	  cgi_puts [cgi_span "class=logo wap" "v [WPCmd PEInfo version].[WPCmd PEInfo revision]"]
	}
      }

      cgi_table_data  class="topHdr" {
	cgi_put [cgi_span "class=sp trans" "style=height:2px;width:5px;" " "]
      }

      cgi_table_data  class="wap topHdr" {
	cgi_division class=hdrContent {
	  # RSS INFO STREAM
	  if {[catch {WPCmd PERss news} news]} {
	    cgi_put [cgi_nbspace]
	    cgi_html_comment "RSS FAILURE: $news"
	  } else {
	    if {[llength $news] > 0} {
	      set style ""
	      set n 0
	      foreach item $news {
		cgi_put [cgi_span class=RSS $style "[cgi_span "class=sp newsImg" "onClick=\"return rotateNews(this);\"" ""][cgi_url [lindex $item 0] [lindex $item 1] "onClick=this.blur();" id=newsItem$n target=_blank]"]
		set style "style=display: none;"
	      }
	    } else {
	    cgi_put [cgi_nbspace]
	    }
	  }

	  # STATUS LINE
	  cgi_division class=wbar id=statusMessage {
	    cgi_division class=status {
	      cgi_division "class=\"flt edge\"" {
		cgi_put [cgi_span "class=sp spsm sm1" ""]
	      }
	      cgi_division "class=\"frt cap\"" {}
	      cgi_division "class=\"frt edge\"" {
		cgi_put [cgi_url [cgi_span "class=sp spsm sm2" ""] "" class=wap "onClick=hideStatusMessage(); return false;"]
	      }
	      cgi_division class=center id=statusMessageText {}
	    }
	  }

	  # WEATHER/USAGE/STATUS
	  cgi_division class=wbar id=weatherBar {
	    # RSS WEATHER
	    cgi_division class=weather id=rssWeather {
	      if {[catch {WPCmd PERss weather} weather]} {
		cgi_html_comment "RSS FAILURE: $weather"
		cgi_put [cgi_nbspace]
	      } else {
		if {[llength $weather] > 0} {
		  set item [lindex $weather 0]
		  cgi_html_comment "index 2 is: [lindex $item 2]"
		  cgi_put [cgi_url [lindex $item 0] [lindex $item 1] "onClick=this.blur();" target=_blank]
		} else {
		  cgi_put [cgi_nbspace]
		}
	      }
	    }
	    if {[info exists _wp(usage)]
		&& 0 == [catch {exec -- $_wp(usage) [WPCmd set env(WPUSER)]} usage]
		&& [regexp {^([0-9]+)[ ]+([0-9]+)$} $usage dummy use_current use_total]
		&& $use_total > 0
		&& $use_current <= $use_total} {
	      cgi_division class=usage {
		cgi_table  width="180px" cellpadding="0" cellspacing="0" {
		  cgi_table_row  {
		    cgi_table_data class=wap width="1%" align=right {
		      cgi_put [cgi_nbspace]
		    }
		    cgi_table_data class=wap width="98%" {

		      set useperc [expr {($use_current * 100) / $use_total}]

		      cgi_table  border=0 width="100%" height="12px" cellspacing=0 cellpadding=0 align=right {
			set percentage [expr {($use_current * 100)/$use_total}]
			cgi_table_row  {
			  cgi_table_data class=wap align=right "style=\"border: 1px solid black; border-right: 0; background-color: #408040;\"" width="$useperc%" {
			    cgi_put [cgi_span "class=sp trans" "style=height:1px;width:1px;" [cgi_span " "]]
			  }
			  cgi_table_data class=wap align=right "style=\"border: 1px solid black; background-color: #ffffff;\"" width="[expr {100 - $useperc}]%" {
			    cgi_put [cgi_span "class=sp trans" "style=height:1px;width:1px;" [cgi_span " "]]
			  }
			}
		      }
		    }
		    cgi_table_data class=wap width="1%" align=right {
		      set units MB
		      if {[info exists _wp(usage_link)]} {
			set txt [cgi_url $use_total $_wp(usage_link) "onClick=this.blur();" title="Detailed usage report" target="_blank"]
		      } else {
			set txt $use_total
		      }

		      cgi_put [cgi_span "style=margin-left: .25em" $txt]
		    }
		  }
		}
	      }
	    }
	  }

	  cgi_division class=pageTitle id=pageTitle {
	    cgi_put $context
	  }

	  cgi_division class=commands {
	    uplevel 1 $cmds
	  }
	}
      }
    }

    cgi_noscript {
      cgi_table_row  {
	cgi_table_data class=noscript  colspan="3" {
	  cgi_put "This version of Web Alpine requires Javascript.  Please enable Javascript, or use the [cgi_url "HTML Version" "$_wp(serverpath)/$_wp(appdir)/wp.tcl"] of Web Alpine"
	}
      }
    }

    cgi_table_row  {
      cgi_table_data id=leftColumn class="wap checkMailandCompose" {
	cgi_table class="toolbarTbl" cellpadding="0" cellspacing="0" {
	  cgi_put "<tbody>"
	  cgi_table_row  {
	    cgi_table_data class="wap" {
	      set nUrl "browse/"
	      if {0 == [regexp {^[0-9]+$} $c]} {
		append nUrl "/0'"
	      } else {
		append nUrl "/$c'"
	      }

	      if {0 == [string length $f]} {
		append nUrl "/INBOX'"
	      } else {
		append nUrl "/[WPPercentQuote $f]"
	      }

	      set nUrl "browse/$c/$f"
	      set onArrival "newMessageList({parms:{'op':'noop'}});"
	      cgi_put [cgi_url [cgi_span "class=sp spmbi spmb15" "Check Mail"] $nUrl title="Check for New Mail" id=gCheck "onClick=return newMailCheck(0);"]
	    }
	    cgi_table_data  class="wap dv1" {
	      cgi_puts [cgi_span "class=sp spmb spmb3" ""]
	    }
	    cgi_table_data class="wap" {
	      set cUrl compose
	      if {[string compare compose $thispage]} {
		switch $thispage {
		  browse  { append cUrl "?pop=browse/$c/$f" }
		  view    { append cUrl "?pop=view/$c/$f/$u" }
		  default { append cUrl "?pop=$thispage" } 
		}
	      }

	      cgi_put [cgi_url [cgi_span "class=sp spmbi spmb16" "Compose"] $cUrl id=composeLink title="Compose New Message" [onClick $cUrl]]
	    }
	  }
	  cgi_put "</tbody>"
	}

	cgi_division class=searchFormDiv {
	  cgi_form [lindex $searchform 0] id=searchForm method=post enctype=multipart/form-data {
	    if {[string length [lindex $searchform 3]]} {
	      set sclick [lindex $searchform 3]
	    } else {
	      set sclick "showStatusMessage('Search is NOT implemented yet',3)"
	    }

	    cgi_text "searchText=Search in [lindex $searchform 1]" class=wap id=searchField title="Click here to search" "onBlur=recallTextField(this, 'Search in [lindex $searchform 1]')" "onClick=\"clearTextField(this, 'Search in [lindex $searchform 1]')\"" "onKeyPress=\"return searchOnEnter(event,'searchButton');\"" maxlength="256"
	    cgi_put "<input alt=\"Search\" name=\"search\" class=\"sp searchBtn\" type=\"submit\" value=\"\" src=\"\" id=\"searchButton\" onClick=\"${sclick}; this.blur(); return false;\" />"

	    set srclass "fld"
	    set searched 0
	    if {[lsearch {browse view} $thispage] >= 0 && [set searched [WPCmd PEMailbox searched]]} {
	      set style ""
	      if {[WPCmd PEMailbox focus]} {
		append srclass " sel"
	      }
	    } else {
	      set style "display:none;"
	    }

	    cgi_division id="searchRefine" style=\"$style\" {
	      cgi_select scope id=searchScope {
		cgi_option "Search within Results" value=narrow
		cgi_option "Add to Search Results" value=broad
		cgi_option "New Search" value=new selected
	      }
	    }
	  }
	}

	if {[lindex $searchform 2]} {
	  cgi_division id=searchAdvance {
	    cgi_puts [cgi_url "Advanced Search" "browse/$c/$f?search=1" class="wap" "onClick=return advanceSearch();"]
	  }
	  cgi_division id=searchClear style=$style {
	    cgi_puts "\[[cgi_url Clear # class="wap" "onClick=return newMessageList({parms:{op:'search',type:'none',page:'new'}});"]\]"
	  }
	}

	cgi_division class=searchFormDiv {cgi_puts [cgi_nbspace]}

	cgi_division id=searchResult class="$srclass" style=\"$style\" {
	  cgi_puts [cgi_url "[cgi_span "class=sp splc splc9" ""][cgi_span class=bld id=searchResultText [cgi_quote_html "Search Result ($searched)"]]" browse/$c/$f?search=1 class=wap "onClick=return listSearchResults();"]
	}

	if {0 == [string compare [lindex $curpage 0] settings]} {
	  uplevel 1 [lindex $curpage 1]
	} else {
	cgi_division class="folderPane" {
	  cgi_anchor_name folders
	  cgi_division class=folderList  {
	    set defc [WPCmd PEFolder defaultcollection]

	    cgi_javascript {
	      cgi_puts "function emptyCurrent(){"
	      cgi_puts "  doEmpty(null,'all');"
	      cgi_puts "}"
	      cgi_puts "function emptyTrash(){"
	      cgi_puts "  emptyFolder('$defc','[lindex [WPCmd PEConfig varget trash-folder] 0]','all',{status:true,fn:'fixupUnreadCount(\"Trash\",0)'});"
	      cgi_puts "}"
	      if {[info exists _wp(spamfolder)]} {
		cgi_puts "function emptyJunk(){"
		cgi_puts "  emptyFolder('$defc','$_wp(spamfolder)','all',{status:true,fn:'fixupUnreadCount(\"Junk\",0)'});"
		cgi_puts "}"
	      }
	    }

	    set current [current_context $thispage $c $f 0 INBOX]
	    cgi_division class="[context_class $current]" id=targetInbox {
	      cgi_put [folder_link $current $thispage 0 INBOX $u [WPCmd PEFolder unread 0 INBOX] [cgi_span "class=sp splc splc1" ""]]
	      if {0 == [string compare browse $thispage] && [string compare -nocase inbox $f]} {
		lappend ddtargets [list targetInbox 0 INBOX]
	      }
	    }

	    set draftf [lindex [WPCmd PEConfig varget postponed-folder] 0]
	    if {0 == [catch {WPCmd PEFolder exists $defc $draftf} result] && $result} {
	      div_folder $thispage $c $f $u $defc Drafts [WPCmd PEFolder unread $defc $draftf] [cgi_span "class=sp splc splc2" ""]
	    }

	    div_folder $thispage $c $f $u $defc Sent [WPCmd PEFolder unread $defc [lindex [WPCmd PEConfig varget default-fcc] 0]] [cgi_span "class=sp splc splc3" ""]

	    if {[info exists _wp(spamfolder)]} {
	      set current [current_context $thispage $c [wpSpecialFolder $c $f] $defc Junk]
	      cgi_division class="[context_class $current]" {
		cgi_put [cgi_span "xclass=left" [folder_link $current $thispage $defc Junk $u [WPCmd PEFolder unread $defc $_wp(spamfolder)] [cgi_span "class=sp splc splc4" ""]]]
		cgi_put [cgi_span "class=right" [empty_link $current Junk $defc]]
	      }
	    }

	    set current [current_context $thispage $c [wpSpecialFolder $c $f] $defc Trash]
	    cgi_division class="[context_class $current]" id=targetTrash {
	      set trashf [lindex [WPCmd PEConfig varget trash-folder] 0]
	      cgi_put [cgi_span class=left [folder_link $current $thispage $defc Trash $u [WPCmd PEFolder unread $defc $trashf] [cgi_span "class=sp splc splc5" id=targetTrashIcon [cgi_span [cgi_nbspace]]]]]
	      cgi_put [cgi_span class=right [empty_link $current Trash $defc]]
	      if {0 == [string compare browse $thispage] && [string compare Trash $f]} {
		lappend ddtargets [list targetTrash $defc $trashf]
	      }
	    }
	    cgi_division class="wap fld" {
	      cgi_put [cgi_nbspace]
	    }

	    cgi_division class="[wpSelectedClass "0 == [string compare $thispage contacts]" 0 "fld"]" id=targetContacts {
	      set cUrl contacts
	      cgi_put [cgi_url "[cgi_span "class=sp splc splc6" ""][cgi_span Contacts]" $cUrl title="Contact List" class=wap [onClick $cUrl]]
	      if {0 == [string compare browse $thispage]} {
		if {0 == [catch {WPCmd PEAddress books} booklist]} {
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

		# pass address book list 
		cgi_puts "<script>setDragTarget('targetContacts',takeAddressFrom,$tAFargs);</script>"
	      }
	    }
	    cgi_division  class="fld" {
	      cgi_put [cgi_nbspace]
	    }
	    cgi_division  class="ftitle bld" {
	      cgi_put "Recent Folders"
	    }

	    if {[catch {WPSessionState left_column_folders} fln]} {
	      set fln $_wp(fldr_cache_def)
	    }

	    set nfl 0

	    foreach fce [getFolderCache] {
	      set fccol [lindex $fce 0]
	      set fcname [lindex $fce 1]
	      if {0 == [catch {WPCmd PEFolder exists $fccol $fcname} result] && $result} {
		set current [current_context $thispage $c $f $fccol $fcname]
		set folderID target$nfl
		cgi_division class="[context_class $current]" id=$folderID {
		  cgi_put [folder_link $current $thispage $fccol $fcname $u [WPCmd PEFolder unread $fccol $fcname] [cgi_span "class=sp splc splc7" id=${folderID}Icon [cgi_span [cgi_nbspace]]]]
		  if {0 == [string compare browse $thispage] && !($c == $fccol && 0 == [string compare $f $fcname])} {
		    lappend ddtargets [list $folderID $fccol $fcname]
		  }
		}
	      }

	      if {[incr nfl] >= $fln} {
		break
	      }
	    }

	    cgi_division class="wap fld" {
	      cgi_put [cgi_nbspace]
	    }
	    cgi_division  class="[wpSelectedClass "0 == [string compare $thispage folders]" 0 "fld"]" {
	      set fUrl "folders"
	      cgi_put [cgi_url "[cgi_span "class=sp splc splc8" ""][cgi_span "View/Manage Folders..."]" $fUrl title="View, Create, Rename, and Delete Folders" class=wap [onClick $fUrl]]
	    }
	  }
	}
      }
      }

      cgi_table_data  class="spc" rowspan="2" {
	cgi_put [cgi_span "class=sp trans" "style=height:2px;width:5px;" [cgi_span " "]]
      }

      cgi_table_data width="100%" valign="top" {
	# pay not attention to the man behind the curtain
	cgi_table class="wap content" cellpadding="0" cellspacing="0" {
	  cgi_puts "<tbody>"
	  cgi_table_row  {
	    cgi_table_data id=topToolbar class=wap {
	      cgi_anchor_name toolbar
	      uplevel 1 $menubar_top
	    }
	  }

	  # display page content
	  cgi_table_row  {
	    cgi_table_data height="100%" valign="top" {
	      cgi_division id=alpineContent {
		uplevel 1 $content
	      }
	    }
	  }

	  cgi_puts "</tbody>"
	}
      }
    }

    cgi_table_row  {
      cgi_table_data {
	cgi_table  class="toolbarTbl" cellpadding="0" cellspacing="0" {
	  cgi_puts "<tbody>"
	  cgi_table_row  {
	    cgi_table_data  class="wap txt" {
	      cgi_put [cgi_nbspace]
	    }
	  }
	  cgi_puts "</tbody>"
	}
      }

      cgi_table_data id=bottomToolbar class=wap  valign="bottom" {
	uplevel 1 $menubar_bottom
      }
    }

    cgi_table_row  {
      cgi_table_data id=ftrContent colspan="3" class="wap footer" align="center" {
	set ft "Powered by [cgi_url Alpine "http://www.washington.edu/alpine/" target="_blank"] - [cgi_copyright] 2007 University of Washington"
	if {0 == [string compare browse $thispage]} {
	  append ft " - [cgi_url "HTML Version" "$_wp(serverpath)/$_wp(appdir)/wp.tcl"]"
	}

	if {[info exists _wp(comments)] && [lsearch {browse view} $thispage] >= 0} {
	  append ft " - [cgi_url "Comments?" "mailto?to=$_wp(comments)&pop=browse/$c/[WPPercentQuote $f]"]"
	}

	cgi_puts $ft
      }
    }
    cgi_puts "</tbody>"
  }

  if {[info exists ddtargets]} {
    cgi_puts "<script>"
    foreach t $ddtargets {
      regsub -all {'} [lindex $t 2] {\'} fn
      cgi_puts "setDragTarget('[lindex $t 0]',dragOntoFolder,{c:'[lindex $t 1]',f:'$fn'});"
    }
    cgi_puts "</script>"
  }
}

proc setCurrentFolder {_c _f _u} {
  global _wp

  upvar 1 $_c c
  upvar 1 $_f f
  upvar 1 $_u u

  # verify current collection/folder
  if {[catch {WPCmd PEFolder current} curfold]} {
    error [list _action browse "cannot determine default folder: $curfold"]
  } else {
    set curc [lindex $curfold 0]
    set curf [lindex $curfold 1]
  }

  # "current" folder's context
  if {$c < 0} {
    set c $curc
  }

  # "current" folder
  if {0 == [string length $f]} {
    set f $curf
  }

  set defc [WPCmd PEFolder defaultcollection]

  # open a different folder?
  if {!($c == $curc && 0 == [string compare $f $curf])} {
    # BUG: DEAL WITH AUTHENTICATION around open below

    # weed out special use folders
    if {0 == $c && 0 == [string compare -nocase $f inbox]} {
      set f INBOX
      set truef INBOX
    } else {
      if {$c == $defc} {
	switch -regexp -- $f {
	  {^Drafts$} {
	    set mode draft
	    catch {WPCmd set pre_draft_folder [WPCmd PEFolder current]}
	    set pf [lindex [WPCmd PEConfig varget postponed-folder] 0]
	    if {[string compare $curf $pf]} {
	      set truef $pf
	      if {[catch {
		if {[WPCmd PEFolder exists $c $pf] <= 0} {
		  WPCmd PEFolder create $c $pf
		}
	      } result]} {
		WPCmd PEInfo statmsg $result
	      }
	    }
	  }
	  {^Trash$} {
	    set mode trash
	    set pf [lindex [WPCmd PEConfig varget trash-folder] 0]
	    if {[string compare $curf $pf]} {
	      set truef $pf
	      if {[catch {
		if {[WPCmd PEFolder exists $c "$pf"] <= 0} {
		  WPCmd PEFolder create $c "$pf"
		}
	      } result]} {
		WPCmd PEInfo statmsg $result
	      }
	    }
	  }
	  {^Junk$} {
	    if {[info exists _wp(spamfolder)]} {
	      set mode junk
	      set pf $_wp(spamfolder)
	      if {[string compare $curf $pf]} {
		set truef $pf
		if {[catch {
		  if {[WPCmd PEFolder exists $c $pf] <= 0} {
		    WPCmd PEFolder create $c $pf
		  }
		} result]} {
		  WPCmd PEInfo statmsg $result
		}
	      }
	    } else {
	      set truef $f
	    }
	  }
	  {^Sent$} {
	    set mode sent
	    set pf [lindex [WPCmd PEConfig varget default-fcc] 0]
	    if {[string compare $curf $pf]} {
	      set truef $pf
	      if {[catch {
		if {[WPCmd PEFolder exists $c $pf] <= 0} {
		  WPCmd PEFolder create $c $pf
		}
	      } result]} {
		WPCmd PEInfo statmsg $result
	      }
	    }
	  }
	  default {
	    set truef $f
	  }
	}
      } else {
	set truef $f
      }
    }

    if {[info exists truef]} {
      if {[catch {eval WPCmd PEMailbox open [list $c $truef]} reason]} {
	error $reason
      } else {
	# do_broach handled this: WPCmd PEInfo statmsg "$f opened with [WPCmd PEMailbox messagecount] messages"

	if {![info exists mode]} {
	  addFolderCache $c $f
	}

	if {[catch {WPCmd PEMailbox trashdeleted current} result]} {
	  WPCmd PEInfo statmsg "Detete FAILURE: $result"
	}
      }
    }
  } else {
    # verify $c $f (and $u if present) exists
    if {!($c == 0 && 0 == [string compare -nocase inbox $f])} {
      if {[catch {WPCmd PEFolder exists $c $f} result]} {
	WPCmd PEInfo statmsg "Cannot test $f for existance: $result"
      } elseif {$result <= 0} {
	WPCmd PEInfo statmsg "Folder $f in collection $c does not exist"
      }
    }

    if {$u > 0 && [catch {WPCmd PEMessage $u number} result]} {
      WPCmd PEInfo statmsg "Message $u does not exist: $result"
    }
  }
}

proc wpFolderMode {c f} {
  if {$c == [WPCmd PEFolder defaultcollection]} {
    switch -exact -- $f {
      Drafts { return draft }
      Sent   { return sent  }
      Junk   { return junk  }
      Trash  { return trash }
    }
  }

  return ""
}

proc wpInitPageFraming {_u _n _mc _ppg _pn _pt} {
  upvar 1 $_u u
  upvar 1 $_n n
  upvar 1 $_mc mc
  upvar 1 $_ppg ppg
  upvar 1 $_pn pn
  upvar 1 $_pt pt

  set focused [WPCmd PEMailbox focus]

  if {$n > 0} {
    set pt [expr {$mc / $ppg}]

    if {$pt < 1} {
      set pt 1
    } elseif {$mc % $ppg} {
      incr pt
    }

    if {$focused} {
      set nth [WPCmd PEMailbox messagecount before $n]
      incr nth
    } else {
      set nth $n
    }

    if {$nth > $ppg} {
      set pn [expr {$nth / $ppg}]
      if {$nth % $ppg} {
	incr pn
      }

      set n [expr {(($pn - 1) * $ppg) + 1}]
      if {$focused} {
	set n [WPCmd PEMailbox next [WPCmd PEMailbox first] $n]
      }
    } else {
      set pn 1
      set n [WPCmd PEMailbox first]
    }

    set u [WPCmd PEMailbox uid $n]
  } else {
    set mc 0
    set pt 1
    set pn 1
  }
}

proc wpHandleAuthException {err c {f ""}} {
  global _wp

  if {[regexp {^CERTQUERY ([^+]+)\+\+(.*)$} $err dummy server reason]} {
    return "code:'CERTQUERY',server:\"${server}\",reason:\"${reason}\""
  } elseif {[regexp {^CERTFAIL ([^+]+)\+\+(.*)$} $err dummy server reason]} {
    return "code:'CERTFAIL',server:\"${server}\",reason:\"${reason}\""
  } elseif {[regexp {^NOPASSWD (.*)$} $err dummy server]} {
    return "code:'NOPASSWD',c:[lindex $c 0],f:\"${f}\",server:\"{${server}/tls}\",reason:\"[lindex $c 1]\",sessid:\"$_wp(sessid)\""
  } elseif {[regexp {^BADPASSWD (.*)$} $err dummy server]} {
    return "code:'BADPASSWD',c:[lindex $c 0],f:\"${f}\",server:\"{${server}/tls}\",reason:\"[lindex $c 1]\",sessid:\"$_wp(sessid)\""
  }

  return {}
}

proc reportAuthException {exp} {
  cgi_put "processAuthException(\{$exp\});"
}

proc wpSaveMenuJavascript {p c f dc onDone {df ""}} {
  set nn 0;
  cgi_put "updateSaveCache(\"$p\",$c,\"$f\",$dc,$onDone,\["
  foreach {oname oval} [getSaveCache $df] {
    if {[incr nn] > 1} {
      cgi_put ","
    }

    cgi_put "{fn:\"$oname\",fv:\"$oval\"}"
  }
  cgi_puts "\]);"
}

proc wpLiteralFolder {c f} {
  global _wp

  if {$c == [WPCmd PEFolder defaultcollection]} {
    switch -exact -- $f {
      Draft {
	return [lindex [WPCmd PEConfig varget postponed-folder] 0]
      }
      Sent  {
	return [lindex [WPCmd PEConfig varget default-fcc] 0]
      }
      Junk  {
	if {[info exists _wp(spamfolder)]} {
	  return $_wp(spamfolder)
	}
      }
      Trash {
	return [lindex [WPCmd PEConfig varget trash-folder] 0]
      }
    }
  }

  return $f
}

proc wpSpecialFolder {c f} {
  global _wp

  if {$c == [WPCmd PEFolder defaultcollection]} {
    if {0 == [string compare $f [lindex [WPCmd PEConfig varget postponed-folder] 0]]} {
      return Draft
    }

    if {0 == [string compare $f [lindex [WPCmd PEConfig varget default-fcc] 0]]} {
      return Sent
    }
    if {[info exists _wp(spamfolder)]} {
      if {0 == [string compare $f $_wp(spamfolder)]} {
	return Junk
      }
    }
    if {0 == [string compare $f [lindex [WPCmd PEConfig varget trash-folder] 0]]} {
      return Trash
    }
  }

  return $f
}

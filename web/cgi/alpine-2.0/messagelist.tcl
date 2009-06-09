# Web Alpine message list painting support
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

#  messagelist
#
#  Purpose:  TCL procedure to produce HTML-based message list of given
#            folder's contents
#
# Input: 
#

proc sort_class {cursort sort} {
  if {[string compare $sort [lindex $cursort 0]]} {
    return blank
  } else {
    return spfcl3
  }
}

proc navListButtons {c f} {
  cgi_table_data  class="wap pageBtns" {
    cgi_puts [cgi_url [cgi_span "class=sp spmb spmbf" [cgi_span "<<"]] "browse/$c/$f" class=wap title="First Page" "onClick=return newMessageList({control:this,parms:{op:'first'}});"]
  }
  cgi_table_data  class="pageBtns" {
    cgi_puts [cgi_url [cgi_span "class=sp spmb spmbp" [cgi_span "<"]] "browse/$c/$f" class=wap title="Previous Page" "onClick=return newMessageList({control:this,parms:{op:'prev'}});"]
  }
  cgi_table_data  class="dv1" {
    cgi_puts [cgi_span "class=sp spmb spmb3" ""]
  }
  cgi_table_data  class="pageBtns" {
    cgi_puts [cgi_url [cgi_span "class=sp spmb spmbn" [cgi_span ">"]] "browse/$c/$f" class=wap title="Next Page" "onClick=return newMessageList({control:this,parms:{op:'next'}});"]
  }
  cgi_table_data  class="pageBtns" {
    cgi_puts [cgi_url [cgi_span "class=sp spmb spmbl" [cgi_span ">>"]] "browse/$c/$f" class=wap title="Last Page" "onClick=return newMessageList({control:this,parms:{op:'last'}});"]
  }
}

proc drawTopListMenuBar {c f} {
  global _wp

  set mode [wpFolderMode $c $f]

  cgi_table class="toolbarTbl" cellpadding="0" cellspacing="0" {
    cgi_put "<tbody>"
    cgi_table_row  {
      if {[info exists mode] && [lsearch [list trash junk] $mode] >= 0} {
	cgi_table_data  class=wap {
	  cgi_puts [cgi_url [cgi_span "class=sp spmbi spmb7" "Delete Forever"] "empty/$c/$f" "onClick=return doEmpty(this,'selected');" "title=Permanently remove messages from $f" class=wap]
	}
	cgi_table_data  class="wap dv1" {
	  cgi_puts [cgi_span "class=sp spmb spmb3" ""]
	}
	cgi_table_data  class=wap {
	  cgi_puts [cgi_url [cgi_span "class=sp spmbi spmb17" "Move to INBOX"] "inbox" "onClick=if(anySelected('Move to INBOX')) newMessageList({control:this,parms:{op:'move',df:'0/INBOX'}}); return false;"  class=wap]
	}
      } else {
	cgi_table_data  class=wap {
	  cgi_puts [cgi_url [cgi_span "class=sp spmbi spmb7" "Delete"] "delete" "onClick= if(anySelected('Delete')) confirmDelete(); this.blur(); return false;" "title=Move message to Trash" class=wap]
	}
	if {(![info exists mode] || [lsearch [list junk draft sent] $mode] < 0) && [info exists _wp(spamfolder)] && [string compare [string tolower $f] $_wp(spamfolder)]} {
	  cgi_table_data  class=wap {
	    cgi_puts [cgi_url [cgi_span "class=sp spmbi spmb8" "Report Spam"] "spam" "onClick=return doSpam(this);"  class=wap]
	  }
	}
	cgi_table_data  class="wap dv1" {
	  cgi_puts [cgi_span "class=sp spmb spmb3" ""]
	}
	cgi_table_data class=wap  {
	  cgi_bullet_list class="menu" {
	    cgi_put "<li class=menuHdr>[cgi_url "More Actions [cgi_img "img/cbn/menu.gif" "class=menuDn menuImg"]" "" "onClick=return false;"]<div>"
	    cgi_bullet_list {
	      if {![info exists mode] || 0 == [string length $mode]} {
		cgi_li [cgi_url "Mark as Read" "read" "onClick=if(anySelected('Mark as Read')) return applyFlag(this,'new','not'); else return false;"]
		cgi_li [cgi_url "Mark as Unread" "unread" "onClick=if(anySelected('Mark as Unread')) return applyFlag(this,'new','ton'); else return false;"]
		set hr "<hr />"
	      } else {
		set hr ""
	      }

	      cgi_li "${hr}[cgi_url "Set Star" "star" "onClick=if(anySelected('Set Star')) return applyFlag(this,'imp','ton'); else return false;"]"
	      cgi_li [cgi_url "Clear Star" "unstar" "onClick=if(anySelected('Clear Star')) return applyFlag(this,'imp','not'); else return false;"]
	      cgi_li "<hr />[cgi_url "Select All" "browse/$c/$f?select=all" "onClick=return selectAllInFolder();"]"
	      cgi_li [cgi_url "Clear All" "browse/$c/$f?select=none" "onClick=return unSelectAllInFolder();"]
	    }
	    cgi_put "</div></li>"
	  }
	}
      }
      if {![info exists mode] || 0 == [string length $mode]} {
	cgi_table_data  class="wap dv1" {
	  cgi_puts [cgi_span "class=sp spmb spmb3" ""]
	}
	cgi_table_data class=wap {
	  cgi_bullet_list class="wap menu" {
	    cgi_put "<li class=menuHdr>[cgi_url "Arrange [cgi_img "img/cbn/menu.gif" "class=menuDn menuImg"]" "" "onClick=return false;"]<div>"
	    cgi_bullet_list class=sortList {
	      if {[catch {PEMailbox sort} cursort]} {
		set cursort [list Arrival 0]
		set rev ""
	      } elseif {[lindex $cursort 1]} {
		set rev Rev
	      } else {
		set rev ""
	      }

	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort From]" [cgi_span [cgi_nbspace]]]Sort by From" "#" id=sortFrom "onClick=return newMessageList({control:this,parms:{op:'sort${rev}From'}});"]
	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort Subject]" [cgi_span [cgi_nbspace]]]Sort by Subject" "#" id=sortSubject "onClick=return newMessageList({control:this,parms:{op:'sort${rev}Subject'}});"]
	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort Date]" [cgi_span [cgi_nbspace]]]Sort by Date" "#" id=sortDate "onClick=return newMessageList({control:this,parms:{op:'sort${rev}Date'}});"]
	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort siZe]" [cgi_span [cgi_nbspace]]]Sort by Size" "#" id=sortsiZe "onClick=return newMessageList({control:this,parms:{op:'sort${rev}Size'}});"]
	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort Arrival]" [cgi_span [cgi_nbspace]]]Sort by Arrival" "#" id=sortArrival "onClick=return newMessageList({control:this,parms:{op:'sort${rev}Arrival'}});"]
	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort tHread]" [cgi_span [cgi_nbspace]]]Group by Thread" "#" id=sorttHread "onClick=return newMessageList({control:this,parms:{op:'sortThread'}});"]
	      cgi_li [cgi_url "[cgi_span "class=sp spfcl [sort_class $cursort OrderedSubj]" [cgi_span [cgi_nbspace]]]Group by Subject" "#" id=sortOrderedSubj "onClick=return newMessageList({control:this,parms:{op:'sortOrderedsubj'}});"]
	    }
	    cgi_put "</div></li>"
	  }
	}
      }
      cgi_table_data  class="wap dv1" {
	cgi_puts [cgi_span "class=sp spmb spmb3" ""]
      }

      # move/cp FOLDER LIST
      cgi_table_data id=listMorcButton class="wap yui-skin-sam yuimenu" {}
      cgi_table_data class=wap {
	cgi_bullet_list class="menu" {
	  cgi_put "<li class=menuHdr>[cgi_url "to Folder [cgi_img "img/cbn/menu.gif" "class=menuDn menuImg"]" "" "onClick=return false;"]<div>"
	  cgi_bullet_list id=browseSaveCache {
	    cgi_li "<hr />[cgi_url "More Folders..." "save/$c/$f" "onClick=if(anySelected('Move or Copy')){ pickFolder('folderList',morcWhich('listMorcButton'),'[PEFolder defaultcollection]',morcInBrowseDone) }; return false;"]"
	  }
	  cgi_put "</div></li>"
	}
      }

      cgi_table_data width="100%" {
	cgi_put [cgi_nbspace]
      }

      cgi_puts [navListButtons $c $f]

      cgi_table_data  class="tbPad" align="right" {
	cgi_put [cgi_nbspace]
      }
    }
    cgi_put "</tbody>"
  }
}

proc drawBottomListMenuBar {c f pn pt mc} {
  cgi_table  class="toolbarTbl" cellpadding="0" cellspacing="0" {
    cgi_put "<tbody>"
    cgi_table_row  {
      cgi_table_data id=listContext  class="wap pageText" {
	cgi_put "Page $pn of $pt [cgi_nbspace]($mc Total Messages)[cgi_nbspace]"
      }
      cgi_table_data  width="100%" {
	cgi_put [cgi_nbspace]
      }

      cgi_puts [navListButtons $c $f]

      cgi_table_data  class="tbPad" align="right" {
	cgi_put [cgi_nbspace]
      }
    }
    cgi_put "</tbody>"
  }
}

proc drawMessageList {c f n ppg} {
    # before getting the list, move anything deleted to Trash
    if {[catch {PEMailbox trashdeleted current} result]} {
      PEInfo statmsg "Trash move Failed: $result"
    }

    # remember message number at top of list
    if {$n > 0 && [catch {PEMailbox current number $n} cm]} {
      set n 1
    }

    set nv [PEMailbox nextvector $n $ppg [list indexparts indexcolor status statuslist]]
    set mv [lindex $nv 0]

    cgi_division id="bannerSelection" {}

    # message list header
    # cells below MUST be reconciled with drawMessageList
    cgi_table class="listTbl" cellpadding="0" cellspacing="0" {
      # index header line
      cgi_put "<thead>"
      cgi_table_row  {
	cgi_table_head class="wap colHdr" align="left" {
	  set nchk 0
	  foreach x $mv {
	    if {[lsearch -exact [lindex [lindex $x 2] 3] selected] >= 0} {
	      incr nchk
	    }
	  }

	  if {$ppg == $nchk} {
	    set allchecked checked
	  } else {
	    set allchecked ""
	  }

	  cgi_checkbox selectall= "onClick=markAllMessages();" $allchecked id=selectall title="Select/Unselect All Messages on this page"
	  cgi_put [cgi_nbspace]
	}

	if {0 == [catch {PEMailbox sort} sort]} {
	  set cursort [lindex $sort 0]
	  set revsort [lindex $sort 1]
	} else {
	  set cursort nonsense
	  set revsort 0
	}

	set iformat [PEMailbox indexformat]
	foreach fmt $iformat {
	  set iname [lindex $fmt 0]
	  set iwidth [lindex $fmt 1]
	  set idname [lindex $fmt 2]

	  if {0 == [string compare -nocase $iname priority]} {
	    continue
	  }

	  # cell defaults
	  set align "left"
	  if {[string compare -nocase $iname $cursort]} {
	    set class "wap colHdr"
	    if {$revsort} {
	      set rev "Rev"
	    } else {
	      set rev ""
	    }
	    set sorturl "browse/$c/[WPPercentQuote $f {/}]?sort=[string tolower $iname]&rev=$revsort"
	    set onclick "onClick=return newMessageList({control:this,parms:{'op':'sort${rev}[string tolower $iname]'}});"
	    set sortextra ""
	  } else {
	    set class "wap selColHdr"
	    if {0 == $revsort} {
	      set rev "Rev"
	      set revbin 1
	    } else {
	      set rev ""
	      set revbin 0
	    }
	    set sorturl "browse/$c/[WPPercentQuote $f {/}]?sort=[string tolower $cursort]&rev=$revbin"
	    set onclick "onClick=return newMessageList({control:this,parms:{'op':'sort${rev}[string tolower $cursort]'}});"
	    if {$revsort} {
	      set sortimg "img/cbn/up.gif"
	      #set sortclass "spml13"
	      set sortimgt "Descending"
	    } else {
	      set sortimg "img/cbn/dn.gif"
	      #set sortclass "spml12"
	      set sortimgt "Increasing"
	    }

	    set sortextra "[cgi_nbspace][cgi_nbspace][cgi_nbspace][cgi_img $sortimg class="wap selectedDn" title="$sortimgt"][cgi_nbspace][cgi_nbspace]"
	  }

	  if {[string length $iwidth]} {
	    set width width="$iwidth"
	  } else {
	    set width ""
	  }

	  switch -- [string tolower $iname] {
	    number {
	      set cd "#"
	    }
	    status {
	      set align center
	      #set cd [cgi_img "img/cbn/info.gif" title="Message Information" "alt=Message Information" class=wap]
	      set cd [cgi_span "class=sp spml spml7" "[cgi_nbspace]"]
	    }
	    attachments {
	      set align center
	      #set cd [cgi_img "img/cbn/attach_sm.gif" title="Attachments"  "alt=Attachments" class=wap]
	      set cd [cgi_span "class=sp spml spml6" "[cgi_nbspace]"]
	    }
	    subject {
	      if {0 == [string compare $f Drafts]} {
		append sortextra "[cgi_nbspace][cgi_nbspace](click to resume composition)"
	      }
	      set cd [cgi_url "${iname}[cgi_nbspace]${sortextra}" $sorturl title="Sort by ${iname}" class=wap $onclick]
	    }
	    default {
	      set cd [cgi_url "${iname}[cgi_nbspace]${sortextra}" $sorturl title="Sort by ${iname}" class=wap $onclick]
	    }
	  }

	  cgi_table_head class="$class" align="$align" $width {
	    cgi_put $cd
	  }
	}

	# fixed priority header
	cgi_table_head  class="colHdr rt" align="center" {
	  cgi_put [cgi_span "class=sp spml spml8" "[cgi_nbspace]"]
	}
      }

      cgi_put "</thead><tbody>"
      set listaction "view/$c/[WPPercentQuote $f {/}]/"
      set viewonclick "onClick=return newMessageText({uid:\$ilu,parms:{page:'new'}});"
      if {$c == [PEFolder defaultcollection] && 0 == [string compare $f Drafts]} {
	set listaction "resume/"
	set viewonclick ""
      }

      if {[set mvl [llength $mv]]} {
	# write index lines
	for {set i 0} {$i < $ppg} {incr i} {
	  if {$i >= $mvl} {
	    if {($i % 2) == 0} {
	      set class "class=ac"
	    } else {
	      set class ""
	    }

	    cgi_table_row $class {
	      cgi_table_data colspan=8 {
		cgi_put [cgi_nbspace]
	      }
	    }

	    continue
	  }

	  set v [lindex $mv $i]

	  set iln [lindex $v 0]
	  set ilu [lindex $v 1]
	  set msg [lindex [lindex $v 2] 0]
	  set linecolor [lindex [lindex $v 2] 1]
	  set stat [lindex [lindex $v 2] 2]
	  set statlist [lindex [lindex $v 2] 3]

	  # set background/bolding
	  if {[lsearch -exact $statlist new] < 0} {
	    set class ""
	    set unread 0
	  } else {
	    set class "unread"
	    set unread 1
	  }

	  # alternating gray lines
	  if {($i % 2) == 0} {
	    if {[string length $class]} {
	      append class " ac"
	    } else {
	      set class "ac"
	    }
	  }

	  if {[string length $class]} {
	    set rc class="$class"
	  } else {
	    set rc ""
	  }

	  if {[lsearch -exact $statlist selected] >= 0} {
	    set checked checked
	    set rid "id=sd"
	  } else {
	    set checked ""
	    set rid "id=line$i"
	  }

	  cgi_table_row $rc $rid {
	    cgi_table_data class=wap {
	      cgi_checkbox uidList=$ilu $checked "onClick=markMessage(this);"
	    }

	    for {set j 0} {$j < [llength $iformat]} {incr j} {

	      set iname [lindex [lindex $iformat $j] 0]

	      set class wap
	      set align left
	      set cd [cgi_nbspace]

	      catch {unset dragit}
	      catch {unset priority}

	      switch -- [string tolower $iname] {
		from -
		to -
		cc -
		sender -
		recipients -
		to {
		  set fstr [lindex [lindex [lindex $msg $j] 0] 0]
		  set flen 20
		  if {[string length $fstr] > $flen} {
		    set fstr "[string range $fstr 0 $flen]..."
		  }

		  set cd [cgi_quote_html $fstr]
		  set dragit $ilu
		}
		subject {
		  if {$i == 0} {
		    cgi_anchor_name messages
		  }

		  set st [lindex $msg $j]

		  if {[llength $st]} {
		    set sstr ""
		    foreach sts $st {
		      # do something with type info, [lindex $sts 2], like threadinfo, HERE
		      append sstr [lindex $sts 0]
		    }

		    set slen 50
		    if {[set sl [string length [string trim $sstr]]]} {

		      if {$sl > $slen} {
			set subtext "[string range $sstr 0 $slen]..."
		      } else {
			set subtext $sstr
		      }
		    } else {
		      set subtext {[Empty Subject]}
		    }
		  } else {
		    set subtext {[Empty Subject]}
		  }

		  if {$unread} {
		    set h1c "class=\"wap unread\""
		  } else {
		    set h1c "class=wap"
		  }

		  set cd [cgi_buffer {cgi_h1 $h1c id=h1$ilu [cgi_url [cgi_quote_html $subtext] ${listaction}$ilu id=ml$ilu class=wap [subst $viewonclick]]}]
		}
		status {
		  set align center
		  catch {unset statclass}
		  if {[lsearch -exact $statlist recent] >= 0 && [lsearch -exact $statlist new] >= 0} {
		    set staticon "new"
		    set statclass spml1
		    set stattitle "New Mail"
		    set statalt "New Mail"
		  } elseif {[lsearch -exact $statlist answered] >= 0} {
		    set staticon "replied"
		    set statclass spml2
		    set stattitle "Replied to Sender"
		    set statalt "Replied"
		  } elseif {[lsearch -exact $statlist forwarded] >= 0} {
		    set staticon "fwd"
		    set statclass spml3
		    set stattitle "Forwarded"
		    set statalt "Forwarded"
		  } elseif {[lsearch -exact $statlist to_us] >= 0} {
		    set staticon "tome"
		    set statclass spml4
		    set stattitle "Addressed to you"
		    set statalt "Addressed to you"
		  } elseif {[lsearch -exact $statlist cc_us] >= 0} {
		    set staticon "ccme"
		    set statclass spml5
		    set stattitle "Cc'd to you"
		    set statalt "Cc'd to you"
		  } else {
		    set staticon "blank"
		    set stattitle ""
		    set statalt ""
		  }

		  if {[info exists statclass]} {
		    set cd [cgi_span "class=sp spml $statclass" "[cgi_nbspace]"]
		  } else {
		    set cd [cgi_nbspace]
		  }
		}
		attachments {
		  set align center
		  if {[regexp {^[0-9]+$} [lindex [lindex [lindex $msg $j] 0] 0]]} {
		    set cd [cgi_span "class=sp spml spml6" "[cgi_nbspace]"]
		  }
		}
		date {
		  set cd "[lindex [lindex [lindex $msg $j] 0] 0]"
		}
		size {
		  set sstr [lindex [lindex [lindex $msg $j] 0] 0]
		  regsub {^\(([^\)]+)\)$} $sstr {\1} sstr
		  set cd "[cgi_quote_html $sstr]"
		}
		number {
		  set cd [WPcomma $iln]
		}
		priority { ; # priority combined with "impotant"
		  set priority [lindex [lindex [lindex $msg $j] 0] 0]
		  unset cd
		}
		default {
		  set cd "[lindex [lindex [lindex $msg $j] 0] 0]"
		}
	      }

	      if {[info exists cd]} {
		if {[info exists dragit]} {
		  set ddid dd$ilu
		  set ddidid id=$ddid
		  set mouseover "onMouseOver=\"cursor('move');\""
		  set mouseout "onMouseOut=\"cursor('default');\""
		} else {
		  set ddidid ""
		  set mouseover ""
		  set mouseout ""
		}

		cgi_table_data align="$align" class="$class" {
		  cgi_division $ddidid $mouseover $mouseout {
		    cgi_put $cd
		  }
		}

		if {[info exists dragit]} {
		  lappend ddlist "'$ddid':'$ilu'"
		}
	      }
	    }

	    # fixed right column for combined important/priority
	    cgi_table_data class="wap rt" {
	      # WARNING: ititle STORES STATE, KEEP IN SYNC WITH lib/browse.js
	      set iclass nostar
	      set ititle ""

	      if {[info exists priority]} {
		switch -regexp $priority {
		  ^2$ -
		  ^high$ {
		    set iclass prihi
		    set ititle "High Priority"
		  }
		  ^1$ -
		  ^highest$ {
		    set iclass prihier
		    set ititle "Highest Priority"
		  }
		}
	      }

	      if {[lsearch -exact $statlist flagged] >= 0} {
		set iclass star
		if {[string length $ititle]} {
		  set ititle "Starred and $ititle"
		} else {
		  set ititle "Starred"
		}
	      }

	      if {0 == [string length $ititle]} {
		set ititle "Set Starred"
	      }

	      cgi_put [cgi_url [cgi_span "class=sp spml $iclass" ""] "star/$c/$f/$ilu" class=mlstat id=star$ilu "title=$ititle" "onClick=return flipStar(this);"]
	    }
	  }
	}
      } else {
	cgi_table_row {
	  cgi_table_data class=wap colspan=8 align=center {
	    cgi_put [cgi_span "style=font-size: large; font-weight: bold ; margin: 50px" "\[ [cgi_quote_html $f] contains no messages \]"]
	  }
	}
      }

      cgi_put "</tbody>"
    }
    if {[info exists ddlist]} {
      cgi_puts "<script>"
      cgi_puts "function loadDDElements(){"
      cgi_puts " var prop, ddList = { [join $ddlist ","] }, ddArray = \[\];"
      cgi_puts " for(prop in ddList) ddArray\[ddArray.length\] = prop;"
      cgi_puts " var tt = new YAHOO.widget.Tooltip('tipDrag', {context:ddArray, showdelay:500, text:'Drag over folder on left to move, Contacts to add'});"
      cgi_puts " for(prop in ddList) canDragit(prop,ddList\[prop\],tt);"
      cgi_puts "}"
      cgi_puts "</script>"
    }
}

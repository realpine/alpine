# $Id: index.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  index.tcl
#
#  Purpose:  CGI script snippet to generate html output associated
#	     with the WebPine message list (index) body frame
#
#  Input:
set index_vars {
    {sort		{}			date}
    {rev		{}			0}
    {top		{}			0}
    {uid		{}			0}
    {width		{}			$_wp(width)}
    {op			{}			none}
    {bod_next		{}			0}
    {bod_last		{}			0}
    {bod_first		{}			0}
    {bod_prev		{}			0}
    {growpage		{}			0}
    {shrinkpage		{}			0}
    {grownum		{}			0}
    {goto		{}			""}
    {gonum		{}			""}
    {select		{}			""}
    {selectop		{}			""}
    {doselect		{}			0}
    {auths		{}			0}
    {user		{}			""}
    {pass		{}			""}
    {create		{}			0}
    {save		{}			""}
    {browse		{}			""}
    {f_name		{}			""}
    {f_colid		{}			""}
    {savecancel		{}			""}
    {promptsave		{}			""}
    {setflag		{}			""}
    {flags		{}			""}
    {repall		{}			0}
    {reptext		{}			0}
    {sortrev		{}			0}
    {sortto		{}			0}
    {sortfrom		{}			0}
    {sortdate		{}			0}
    {sortsize		{}			0}
    {sortsubject	{}			0}
    {sortorderedsubj	{}			0}
    {sortthread		{}			0}
    {queryexpunge	{}			0}
    {expunge		{}			0}
    {cid		{}			0}
    {folders		{}			0}
    {compose		{}			0}
    {submitted		{}			0}
    {zoom		{}			""}
    {mark		{}			""}
    {unmark		{}			""}
    {search		{}			""}
    {aggon		{}			""}
    {aggoff		{}			""}
    {hdron		{}			""}
    {hdroff		{}			""}
    {split		{}			0}
    {spamit		{}			""}
    {reload}
}

#  Output:
#

# FOR TESTING VARIOUS LAYOUTS AND SUCH
set do_status_icons 1

set selectverb Search

set growmax 50
set growverb Increase
set shrinkverb Decrease

# various color defs
set color(sortfg)  "#FFFFFF"
set color(sortbg)  "#BFBFBF"
set color(line1)   "#EEEEEE"
set color(line2)   "#FFFFFF"
set color(greyout) "#888888"
set sb_width 13
set sb_dir bar

set aggbar	1
set sortbar	2

set gonum ""

if {[catch {WPCmd PEInfo indexlines} ppg] || $ppg <= 0} {
  set ppg $_wp(indexlines)
}

# make sure any caching doesn't screw this setting
catch {WPCmd PEInfo set wp_spec_script fr_index.tcl}

proc statmsg {s} {
  global newmail

  lappend newmail [list $s]
}

proc selectresponse {type num prevnum zoomref topref uidref} {
  upvar $zoomref zoomed
  upvar $topref top
  upvar $uidref uid

  if {$num == 0} {
    if {$prevnum} {
      set statmsg "$type search found no additional messages.  Set of marked messages unchanged"
    } else {
      set statmsg "$type search found no matching messages"
    }
  } elseif {$num > 0} {
    if {$prevnum} {
      set statmsg "$type search found $num messages. [expr {$num + $prevnum}] total messages now marked."
    } else {
      set statmsg "$type search found and marked $num messages"
    }

    if {!$zoomed} {
      # force reframing
      set top "0+0"
      set uid 0
    }
  } else {
    set statmsg "[set num [expr abs($num)]] messages Unmarked."
    if {$prevnum > $num} {
      append statmsg " [expr {$prevnum - $num}] remain Marked."
    }
  }

  # update zoomed count or zoom  if necessary
  if {$zoomed} {
    set zoomed [WPCmd PEMailbox selected]
  } elseif {[WPCmd PEInfo feature auto-zoom-after-select] && [WPCmd PEMailbox selected]} {
    set zoomed [WPCmd PEMailbox zoom 1]
    #append statmsg ". Those not matching excluded from view."
  }

  statmsg $statmsg
  catch {WPCmd PEInfo unset wp_def_search_text}
}


proc sortname {name {current 0}} {
  global rev me

  switch -- $name {
    Number { set newname "#" }
    OrderedSubj { set newname "Ordered Subject" }
    Arrival { set newname Arv }
    Status { set newname "&nbsp;" }
    default { set newname $name }
  }

  if {$current} {
    if {$rev > 0} {
      set text [cgi_imglink increas]
      set args rev=0
    } else {
      set text [cgi_imglink decreas]
      set args rev=1
    }

    append newname [cgi_url $text "wp.tcl?page=index&sortrev=1" "title=Reverse $newname ordering" target=body]
  }

  return $newname
}

proc lineclass {linenum} {
  if {$linenum % 2} {
    return i0
  } else {
    return i1
  }
}

proc uid_framed {u mv} {
  foreach m $mv {
    if {$u == [lindex $m 1]} {
      return 1
    }
  }
  return 0
}

proc index_quote {text} {
  set text [cgi_quote_html $text]
  regsub -all { } $text {\&nbsp;} text

  return $text
}

proc index_part_color {text color} {
  if {[llength $color] == 2} {
    set fg [lindex $color 0]
    set bg [lindex $color 1]
    return [cgi_span "style=color: $fg; background-color: $bg" $text]
  } else {
    return $text
  }
}

## read vars
foreach item $index_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

# check for new mail first since counts
# and sort order might change...
if {[catch {WPNewMail $reload ""} newmail]} {
  error [list _action "new mail" $newmail]
}

set cid [WPCmd PEInfo key]

set messagecount [WPCmd PEMailbox messagecount]
set delcount [WPCmd PEMailbox flagcount deleted]
set flagcmd [WPCmd PEInfo feature enable-flag-cmd]
set aggops [WPCmd PEInfo feature enable-aggregate-command-set]
set aggtabstate [WPCmd PEInfo aggtabstate]
set zoomed [WPCmd PEMailbox zoom]

set indexheight [WPIndexLineHeight]

if {$split != "0"} {
  set vtarget fr_bottom
} else {
  set vtarget spec
}

# perform any requested actions
if {$queryexpunge == 1 || [string compare [string tolower $queryexpunge] expunge] == 0} {
  if {$delcount > 0} {
    set fn [WPCmd PEMailbox mailboxname]
    set oncancel index
    # delcount, messagecount set above
    source [WPTFScript expunge]
    set nopage 1
  } else {
    statmsg "No deleted messages to Expunge"
  }
} elseif {$expunge == 1
	  || [string compare [string tolower $expunge] expunge] == 0 
	  || [string compare [string range [string tolower $expunge] 0 10] "yes, remove"] == 0} {
  if {$delcount > 0} {
    if {$delcount < $messagecount
	|| ($delcount == $messagecount
	    && 0 == [catch {cgi_import emptyit}]
	    && 1 == $emptyit)} {
      set setflags [WPCmd PEMessage $top status]
      if {[lsearch $setflags "Deleted"] != -1} {
	set curmsg [WPCmd PEMessage $top number]
	set nextmsg [WPCmd PEMailbox next $curmsg]
	set done 0
	while {$nextmsg > $curmsg && $done == 0} {
	  set nextuid [WPCmd PEMailbox uid $nextmsg]
	  set setflags [WPCmd PEMessage $nextuid status]
	  if {[lsearch $setflags "Deleted"] == -1} {
	    set uid $nextuid
	    set top $uid
	    set done 1
	  } else {
	    set curmsg $nextmsg
	    set nextmsg [WPCmd PEMailbox next $curmsg]
	  }
	}

	if {$done == 0} {
	  set curmsg [WPCmd PEMessage $top number]
	  set prevmsg [WPCmd PEMailbox next $curmsg -1]
	  while {$prevmsg < $curmsg && $done == 0} {
	    set prevuid [WPCmd PEMailbox uid $prevmsg]
	    set setflags [WPCmd PEMessage $prevuid status]
	    if {[lsearch $setflags "Deleted"] == -1} {
	      set uid $prevuid
	      set top $uid
	      set done 1
	    } else {
	      set curmsg $prevmsg
	      set prevmsg [WPCmd PEMailbox next $curmsg -1]
	    }
	  }
	}
      }

      if {[catch {WPCmd PEMailbox expunge} blasted] || [string length $blasted]} {
	statmsg "Expunge problem: $blasted"
      } else {
	set delcount 0
	set messagecount [WPCmd PEMailbox messagecount]
	if {$messagecount < 1} {
	  set uid 0
	  set top 0
	  set first 1
	  if {$zoomed} {
	    WPCmd PEMailbox zoom 0
	  }
	} else {
	  if {$top > 0 && ([catch {WPCmd PEMessage $top number} n] || $n <= 0)} {
	    # previous top message is gone, figure new one below
	    set top 0
	  }

	  if {$uid > 0 && ([catch {WPCmd PEMessage $uid number} n] || $n <= 0)} {
	    # recently viewed message is gone
	    set uid 0
	  }
	}
      }
    } else {
      statmsg "<div style=\"width: 75%; background-color: pink; border: 1px solid red; padding: 0 8\"><span style=\"font-size: 12pt; color: white; background-color: red; padding: 0 5\">No Messages Expunged!</span><div style=\"font-weight: normal; padding: 4 8\">To prevent unintended deletions, you must check the box on the Expunge Confirmation page to indicate that you acknowledge expunge will leave the folder <b>[WPCmd PEMailbox mailboxname]</b> empty.</div></div>"
    }
  } else {
    statmsg "No deleted messages to Expunge"
  }
} elseif {$growpage == 1 || [string compare $growverb $growpage] == 0} {
  if {$grownum <= 0 || $grownum > $growmax} {
    set grownum 5
  }

  incr ppg $grownum
  catch {WPCmd PEInfo set grownum $grownum}
  WPCmd PEInfo indexlines $ppg
} elseif {$shrinkpage == 1 || [string compare $shrinkverb $shrinkpage] == 0} {
  if {$grownum <= 0 || $grownum > $growmax} {
    set grownum 5
  }

  incr ppg -$grownum
  catch {WPCmd PEInfo set grownum $grownum}

  if {$ppg < 1} {
    set ppg 1
  }

  WPCmd PEInfo indexlines $ppg
} elseif {$bod_prev} {
  set top "$top-$ppg"
  set uid 0
} elseif {$bod_first} {
  set first 1
  if {$messagecount > 0} {
    set top [WPCmd PEMailbox uid $first]
  }

  set uid 0
} elseif {$bod_next} {
  set top "$top $ppg"
  set uid 0
} elseif {$bod_last} {
  if {$messagecount > 0} {
    if {$zoomed} {
      if {$zoomed > $ppg} {
	if {[catch {WPCmd PEMailbox uid [WPCmd PEMailbox next [expr {[WPCmd PEMailbox last] - [expr {${ppg} - 1}]}]]} top]} {
	  set first [WPCmd PEMailbox first]
	  set top [WPCmd PEMailbox uid $first]
	} else {
	  set first [WPCmd PEMessage $top number]
	}
      }
    } else {
      if {[set first [expr {$messagecount - $ppg + 1}]] < 0} {
	set first 1
      }

      set top [WPCmd PEMailbox uid $first]
    }
  }

  set uid 0
} elseif {[string length $goto]} {
  if {[regexp {^([0-9]+)$} $gonum n]} {
    if {$n > 0 && $n <= [WPCmd PEMailbox last]} {
      set first $n
      set uid [WPCmd PEMailbox uid $first]
      set top $uid
      set gonum ""
    } else {
      statmsg "Jump value $gonum out of range"
      set goto ""
    }
  } else {
    if {[string length $gonum]} {
      statmsg "Unrecognized Jump value: $gonum"
    } else {
      statmsg "Enter a message number, then click 'Jump'"
    }
  }
} elseif {$promptsave == 1 || [string compare [string tolower $promptsave] save] == 0} {
  if {[WPCmd PEMailbox selected] == 0} {
    statmsg "Place checkmarks in the box next to desired messages, then click 'Save'"
  } else {
    set uid 0
    source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_promptsave.tcl]
    set nopage 1
  }
} elseif {$savecancel == 1 || [string compare cancel [string tolower $savecancel]] == 0} {
  catch {WPCmd PEInfo unset wp_index_script}
  lappend newmail [list "Save cancelled. Folder not created."]
} elseif {[string length $browse] && [string compare $browse Browse] == 0} {
  set uid 0
  _cgi_set_uservar onselect main
  _cgi_set_uservar oncancel main
  _cgi_set_uservar controls 0
  source [WPTFScript savebrowse]
  set nopage 1
} elseif {([string length $save] && ([string compare [string trim $save] OK] == 0 || [string compare [string trim $save] Save] == 0)) || [string compare save [string tolower $op]] == 0} {
  if {[WPCmd PEMailbox selected] == 0} {
    statmsg "Place checkmarks in the box next to desired messages, then click 'Save'"
  } elseif {[catch {cgi_import cancel}] == 0} {
    statmsg "Save cancelled"
  } else {
    set f_name [string trim $f_name]
    if {[string length $f_name]} {
      if {[regexp {^([0-9]+)$} $f_colid]} {
	switch -exact -- $f_name {
	  __folder__prompt__ {
	    set uid 0
	    _cgi_set_uservar onselect {index save=OK}
	    _cgi_set_uservar oncancel index
	    _cgi_set_uservar target body
	    _cgi_set_uservar controls 0
	    source [WPTFScript savecreate]
	    set nopage 1
	  }
	  __folder__list__ {
	    set uid 0
	    _cgi_set_uservar onselect {index save=OK}
	    _cgi_set_uservar oncancel index
	    _cgi_set_uservar target body
	    _cgi_set_uservar controls 0
	    source [WPTFScript savebrowse]
	    set nopage 1
	  }
	  default {
	    if {$auths} {
	      catch {WPCmd PESession nocred $f_colid $f_name}
	      if {[catch {WPCmd PESession creds $f_colid $f_name $user $pass} result]} {
		lappend newmail ["Cannot set credentials ($f_colid) $f_name: result"]
	      }
	    }

	    if {[catch {WPCmd PEFolder exists $f_colid $f_name} reason]} {
	      if {[string compare BADPASSWD [string range $reason 0 8]] == 0} {
		set oncancel "index.tcl&savecancel=1"
		set conftext "Create Folder '$f_name'?"
		lappend params [list page index]
		lappend params [list save Save]
		lappend params [list f_name $f_name]
		lappend params [list f_colid $f_colid]
		source [WPTFScript auth]
		set nopage 1
	      } else {
		lappend newmail [list "Existance test failed: $reason"]
	      }
	    } elseif {$reason == 0} {
	      if {$create == 1 || [string compare create [string tolower $create]] == 0} {
		if {[catch {WPCmd PEFolder create $f_colid $f_name} reason]} {
		  lappend newmail [list "Create failed: $reason"]
		} else {
		  set dosave 1
		}
	      } else {
		set qstate [list $f_name]
		set params [list [list page index]]
		lappend params [list save OK]
		lappend params [list f_name $f_name]
		lappend params [list f_colid $f_colid]
		lappend qstate $params

		if {[catch {WPCmd PEInfo set querycreate_state $qstate}] == 0} {
		  source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_querycreate.tcl]
		  set nopage 1
		} else {
		  statmsg "Error saving creation state"
		}	    
	      }
	    } else {
	      set dosave 1
	    }

	    if {[info exists dosave]} {
	      if {[catch {WPCmd PEMailbox apply save $f_colid $f_name} reason]} {
		error [list _action "Cannot save to $f_name in $f_colid" $reason]
	      } else {
		set statmsg "Saved $reason message[WPplural $reason] to $f_name"

		set savedef [WPTFSaveDefault $uid]
		if {[lindex $savedef 0] == $f_colid} {
		  WPTFAddSaveCache $f_name
		}

		if {[WPCmd PEInfo feature auto-unselect-after-apply]} {
		  if {[catch {WPCmd PEMailbox select none} result]} {
		    set statmsg "Cannot clear all message marks: $result"
		  } else {
		    if {$result == 0} {
		      set statmsg "No Marked messages to Unmark"
		    } else {
		      append statmsg " and unmarked"
		    }
		  }
		}

		statmsg $statmsg
	      }
	    }
	  }
	}
      } else {
	statmsg "Unrecognized collection id"
      }
    } else {
      statmsg "Must provide a folder name to Save to"
    }
  }
} elseif {[string compare Set $setflag] == 0 || [string compare Delete $setflag] == 0 || [string compare Undelete $setflag] == 0} {
  switch -- $setflag {
    Delete {
      set flagverb Delete
      set flags delete
    }
    Undelete {
      set flagverb Undelete
      set flags undeleted
    }
    default {
      set flagverb Set
    }
  }

  if {[WPCmd PEMailbox selected] == 0} {
    statmsg "Place checkmarks in the box next to desired messages, then click '$flagverb'"
  } else {
    switch -- $flags {
      delete {
	set flagverb "for deletion"
	if {[catch {WPCmd PEMailbox apply delete} reason]} {
	  set statmsg "Problem deleting: $reason"
	}
      }
      undeleted {
	set flagverb Undeleted
	if {[catch {WPCmd PEMailbox apply undelete} reason]} {
	  set statmsg "Problem undeleting: $reason"
	}
      }
      new {
	set flagverb New
	if {[catch {WPCmd PEMailbox apply flag ton new} reason]} {
	  set statmsg "Problem setting New flag: $reason"
	}
      }
      read {
	set flagverb Read
	if {[catch {WPCmd PEMailbox apply flag not new} reason]} {
	  set statmsg "Problem Unsetting New flag: $reason"
	}
      }
      answered {
	set flagverb Answered
	if {[catch {WPCmd PEMailbox apply flag ton ans} reason]} {
	  set statmsg "Problem setting Answered flag: $reason"
	}
      }
      unanswered {
	set flagverb "Not Answered"
	if {[catch {WPCmd PEMailbox apply flag not ans} reason]} {
	  set statmsg "Problem Unsetting Answered flag: $reason"
	}
      }
      important {
	set flagverb "Important"
	if {[catch {WPCmd PEMailbox apply flag ton imp} reason]} {
	  set statmsg "Problem setting Important flag: $reason"
	}
      }
      unimportant {
	set flagverb "Not Important"
	if {[catch {WPCmd PEMailbox apply flag not imp} reason]} {
	  set statmsg "Problem Unsetting Important flag: $reason"
	}
      }
      default {
	statmsg "Unknown flag set request: $flags"
      }
    }

    if {![info exists statmsg]} {
      set statmsg "$reason message[WPplural $reason] flagged $flagverb"
      if {[WPCmd PEInfo feature auto-unselect-after-apply]} {
	if {[catch {WPCmd PEMailbox select none} result]} {
	  set statmsg "Cannot clear all message marks: $result"
	} else {
	  if {$result == 0} {
	    set statmsg "No Selected messages to Unmark"
	  } else {
	    append statmsg " and unmarked"
	  }
	}
      }
    }

    statmsg $statmsg
  }
} elseif {[string length $mark]} {
  if {[string compare mark [string tolower [lindex $mark 0]]] == 0} {
    if {[catch {WPCmd PEMailbox select all} result]} {
      statmsg "Cannot mark all messages: $result"
    } else {
      set zm ""
      if {$zoomed} {
	set zoomed [WPCmd PEMailbox zoom 0]
	set zm ". Message List now displaying all messages."
      }
      statmsg "All messages in folder '[WPCmd PEMailbox mailboxname]' marked$zm"
    }
  } elseif {[string compare $mark 1] == 0} {
    if {$zoomed} {
      statmsg "Messages on page already marked"
    } elseif {$messagecount > 0} {
      set p [split $uidpage ","]
      set s [WPCmd PEMessage [lindex $p 0] number]
      set e [WPCmd PEMessage [lindex $p end] number]

      if {[catch {WPCmd PEMailbox select broad num $s $e} result]} {
	statmsg "Cannot mark messages: $result"
      } else {
	statmsg "Marked all messages [cgi_underline "on this page"]"
      }
    }
  }
} elseif {[string length $unmark]} {
  if {[string compare unmark [lindex [string tolower $unmark] 0]] == 0} {
    if {[catch {WPCmd PEMailbox select none} result]} {
      statmsg "Cannot clear all message marks: $result"
    } else {
      if {$result == 0} {
	statmsg "No marked messages to Unmark"
      } else {
	if {[regexp {[0-9]+} $result] && $result == $messagecount} {
	  set result All
	}

	statmsg "Unmarked $result Messages"
      }
    }
  } elseif {[string compare $unmark 1] == 0} {
    if {[catch {WPCmd PEMailbox select clear [split $uidpage ","]} result]} {
      statmsg "Cannot clear message marks: $result"
    } else {
      if {$result == 0} {
	statmsg "No marked messages to Unmark"
      } else {
	statmsg "Unmarked all messages [cgi_underline "on this page"]"
      }
    }
  }
} elseif {[string length $select] && [string compare $select $selectverb] == 0} {
  switch -- $selectop {
    mark {
      if {[catch {WPCmd PEMailbox select all} result]} {
	statmsg "Cannot mark all messages: $result"
      } elseif {0} {
	statmsg "All messages marked"
      }
    }
    clear {
      if {[catch {WPCmd PEMailbox select none} result]} {
	statmsg "Cannot clear all message marks: $result"
      } elseif {0} {
	if {$result == 0} {
	  statmsg "No marked messages to Unmark"
	} else {
	  statmsg "Unmarked $result messages"
	}
      }
    }
    text {
      set uid 0
      source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_seltext.tcl]
      set nopage 1
    }
    date {
      set uid 0
      source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_seldate.tcl]
      set nopage 1
    }
    stat {
      set uid 0
      source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_selstat.tcl]
      set nopage 1
    }
    zoom {
      if {[WPCmd PEMailbox selected] == 0} {
	statmsg "No marked messages to Zoom on"
      } elseif {$zoomed == 0} {
	set zoomed [WPCmd PEMailbox zoom 1]
      }
    }
    unzoom {
      set zoomed [WPCmd PEMailbox zoom 0]
    }
    null {
    }
    default {
      set uid 0
      source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_select.tcl]
      set nopage 1
    }
  }
} elseif {$zoom != ""} {
  set z 0
  if {[WPCmd PEMailbox selected] == 0} {
    if {[string compare [string tolower [string range $zoom 0 7]] "show all"]} {
      statmsg "No marked messages to Zoom in on!"
    }
  } elseif {$zoomed == 0} {
    set z 1
  }

  set zoomed [WPCmd PEMailbox zoom $z]
} elseif {$doselect == 1} {
  WPLoadCGIVar result

  if {[string compare new [string tolower $result]] == 0} {
    catch {WPCmd PEMailbox select none}
    catch {WPCmd PEMailbox zoom 0}
    set zoomed 0
    set result broad
  }

  set selected [WPCmd PEMailbox selected]

  if {[catch {WPLoadCGIVar cancel}] || [string compare cancel [string tolower $cancel]] != 0} {
    if {[catch {WPLoadCGIVar by}]} {
      if {[string length $selectop]} {
	set by [string tolower [lindex $selectop 1]]
      } else {
	statmsg "Unknown Search Option -- Click button associated with desired search"
	set by unset
      }
    }

    switch -- $by {
      date {
	WPLoadCGIVar datecase ;# on, since or before
	WPLoadCGIVar datemon  ;# month abbrev: jan, feb etc
	WPLoadCGIVar dateday  ;# day num
	WPLoadCGIVar dateyear ;# year num

	if {[catch {WPCmd PEMailbox select $result date $datecase $dateyear $datemon $dateday} reason]} {
	  statmsg "Date Search Failed: $reason"
	} else {
	  selectresponse Date $reason $selected zoomed top uid
	}
      }
      text {
	WPLoadCGIVar textcase ;# ton, not
	WPLoadCGIVar field    ;# to from cc recip partic subj any
	WPLoadCGIVar text     ;# search string

	if {[catch {WPCmd PEMailbox select $result text $textcase $field $text} reason]} {
	  statmsg "Text Search Failed: $reason"
	} else {
	  switch -- $field {
	    subj {set fieldtext "\"Subject:\""}
	    from {set fieldtext "\"From:\""}
	    to {set fieldtext "\"To:\""}
	    cc {set fieldtext "\"Cc:\""}
	    recip {set fieldtext "Recipient"}
	    partic {set fieldtext "Participant"}
	    default {set fieldtext "Full text"}
	  }

	  selectresponse $fieldtext $reason $selected zoomed top uid
	}
      }
      status {
	if {[catch {WPLoadCGIVar flag}]} {
	  statmsg "Choose a Status value and then click [cgi_bold "Search Status"]"
	} else {
	  WPLoadCGIVar statcase
	  if {[catch {WPCmd PEMailbox select $result status $statcase $flag} reason]} {
	    statmsg "Flag Search Failed: $reason"
	  } else {
	    switch -- $statcase {
	      not  { set casetext "NOT " }
	      default  { set casetext "" }
	    }
	    switch -- $flag {
	      imp {set flagtext Important}
	      new {set flagtext "New status"}
	      ans {set flagtext Answered}
	      del {set flagtext Deleted}
	    }
	    selectresponse "${casetext}${flagtext} status" $reason $selected zoomed top uid
	  }
	}
      }
      unset {
	if {[catch {WPLoadCGIVar text}] == 0} {
	  catch {WPCmd PEInfo set wp_def_search_text $text}
	}
      }
      default {
	WPCmd PEInfo statmsg "Unknown Search Option"
      }
    }
  } else {
    catch {WPCmd PEInfo unset wp_def_search_text}
  }

  catch {WPCmd PEInfo unset wp_index_script}
} elseif {$sortrev} {
  if {$rev} {
    set rev 0
  } else {
    set rev 1
  }
} elseif {$sortto} {
  set sort To
} elseif {$sortfrom} {
  set sort From
} elseif {$sortdate} {
  set sort Date
} elseif {$sortsize} {
  set sort siZe
} elseif {$sortsubject} {
  set sort Subject
} elseif {$sortorderedsubj} {
  set sort OrderedSubj
} elseif {$sortthread} {
  set sort tHread
} elseif {$folders} {
  source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) folders.tcl]
  set nopage 1
} elseif {[string compare reply [string tolower $op]] == 0} {
  if {0} {
    set oncancel index
    set cid [WPCmd PEInfo key]
    #_cgi_set_uservar style Reply
    source [WPTFScript compose]
    set nopage 1
  }
  statmsg "Aggregate Reply not implemented yet"
} elseif {[string compare forward [string tolower $op]] == 0} {
  statmsg "Aggregate Forward not implemented yet"
} elseif {[string length $aggon]} {
  set aggtabstate [expr {$aggtabstate | $aggbar}]
  WPCmd PEInfo aggtabstate $aggtabstate
} elseif {[string length $aggoff]} {
  if {$zoomed} {
    set zoomed [WPCmd PEMailbox zoom 0]
    statmsg "Message List now displaying all (marked and unmarked) messages"
  }

  set aggtabstate [expr {$aggtabstate & ~$aggbar}]
  WPCmd PEInfo aggtabstate $aggtabstate
} elseif {[string length $hdron]} {
  set aggtabstate [expr {$aggtabstate | $sortbar}]
  WPCmd PEInfo aggtabstate $aggtabstate
} elseif {[string length $hdroff]} {
  set aggtabstate [expr {$aggtabstate & ~$sortbar}]
  WPCmd PEInfo aggtabstate $aggtabstate
} elseif {[catch {WPCmd PEInfo set wp_index_script} script] == 0} {
  catch {WPCmd PEInfo unset wp_index_script}
  set uid 0
  source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) $script]
  set nopage 1
} elseif {[string length $spamit]} {
  if {[WPCmd PEMailbox selected] == 0} {
    statmsg "Place checkmarks in the box next to desired messages, then click '$spamit'"
  } else {
    # create

    # aggregate save 
    if {[info exists _wp(spamsubj)] && [string length $_wp(spamsubj)]} {
      set spamsubj $_wp(spamsubj)
    } else {
      set spamsubj "Spam Report"
    }

    # aggregate delete
    if {[info exists _wp(spamfolder)] && [string length $_wp(spamfolder)]
	&& [catch {
	  set savedef [WPCmd PEMailbox savedefault]
	  if {[WPCmd PEFolder exists [lindex $savedef 0] $_wp(spamfolder)] == 0} {
	    WPCmd PEFolder create [lindex $savedef 0] $_wp(spamfolder)
	  }

	  WPCmd PEMailbox apply save [lindex $savedef 0] $_wp(spamfolder)
	} result]} {
      statmsg "Error Reporting Spam: $result"
    } elseif {[info exists _wp(spamaddr)] && [string length $_wp(spamaddr)]
	      && [catch {WPCmd PEMailbox apply spam $_wp(spamaddr) $spamsubj} reason]} {
      statmsg "Error Sending Spam Notice: $reason"
    } elseif {[catch {WPCmd PEMailbox apply delete} reason]} {
      statmsg "Error marking Spam Deleted: $reason"
    } else {
      set seld [WPCmd PEMailbox selected]
      statmsg "$seld spam message[WPplural $seld] reported and flagged for deletion"
      catch {WPCmd PEMailbox select none}
    }
  }
} elseif {[string compare "set flags" [string tolower $op]] == 0} {
  if {[catch {WPCmd PEInfo flaglist} flags]} {
    statmsg "Can't get flags: $flags"
  } else {
      # then go thru flag list setting and clearing as needed
    foreach flag $flags {
      if {[catch {cgi_import_as $flag val}]} {
	if {[catch {WPCmd PEMessage $uid flag $flag 0} result]} {
	  statmsg "Can't set $flag: $result"
	  break
	}
      } else {
	set newval [expr {[string compare $val on] == 0}]
	if {[catch {WPCmd PEMessage $uid flag $flag $newval} result]} {
	  statmsg "Can't set $flag: $result"
	  break
	}
      }
    }
  }
}

if {![info exists nopage]} {
  # set the specified sort order
  if {[catch {WPCmd PEMailbox sort $sort $rev} currentsort]} {
    error [list _action Sort $currentsort]
  }

  if {$aggops} {
    set selected [WPCmd PEMailbox selected]
    if {$selected < 1 && $zoomed} {
      set zoomed 0
      WPCmd PEMailbox zoom 0
      statmsg "Message List now displaying all (marked and unmarked) messages"
    }
  }

  # "top" is uid of first message in index.  n after '+' is relative offset
  if {$messagecount <= 0} {
    set first 1
    set top 0
    set miv ""
  } else {
    if {[regexp {^([0-9]+)([\+\ -])([0-9]+)$} $top dummy u sign offset]} {
      if {$u == 0 || [catch {WPCmd PEMessage $u number} first]} {
	set first [WPCmd PEMailbox first]
	set top [WPCmd PEMailbox uid $first]
      } else {
	set top $u
      }

      if {$offset != 0} {
	if {[catch {WPCmd PEMailbox next [WPCmd PEMessage $top number] ${sign}${offset}} newfirst] == 0} {
	  set first $newfirst
	  set top [WPCmd PEMailbox uid $newfirst]
	}
      }
    } elseif {[regexp {^[0-9]*$} $top]} {
      if {$top == 0 || [catch {WPCmd PEMessage $top number} first]} {
	set first [WPCmd PEMailbox first]
	set top [WPCmd PEMailbox uid $first]
      }
    } else {
      statmsg "Invalid UID for first message"
      set first [WPCmd PEMailbox first]
      set top [WPCmd PEMailbox uid $first]
    }

    # validate first is in range
    if {$messagecount && $first < [set f [WPCmd PEMailbox first]]} {
      set first $f
      set top [WPCmd PEMailbox uid $f]
    }

    # check framing
    if {$zoomed} {
      if {$zoomed < $ppg || [WPCmd PEMessage $top select] == 0} {
	set first [WPCmd PEMailbox first]
	set top [WPCmd PEMailbox uid $first]
      } else {
	set first [WPCmd PEMessage $top number]
      }

      set uid $top
    } else {
      if {$messagecount < $ppg} {
	if {![string length $goto]} {
	  set first [WPCmd PEMailbox first]
	  set top [WPCmd PEMailbox uid $first]
	}
      } elseif {$first > $messagecount} {
	if {[set mdiff [expr {$messagecount - $ppg + 1}]] < 0} {
	  set first [WPCmd PEMailbox first]
	  set top [WPCmd PEMailbox uid $first]
	} else {
	  set first $mdiff
	  set top [WPCmd PEMailbox uid $mdiff]
	}
      }
    }

    # validate uid
    # use "size" instead of "number" to work around temporary bug in pinetcld
    if {$uid != 0 && ([catch {WPCmd PEMessage $uid size} n] || $n <= 0)} {
      set uid 0
    }

    set nv [WPCmd PEMailbox nextvector $first $ppg [list indexparts indexcolor status statusbits]]
    set miv [lindex $nv 0]
    set charset [lindex $nv 1]

    # hook to keep last viewed message in current message list
    if {$uid > 0} {
      if {[catch {WPCmd PEMessage $uid number} n] == 0 && [uid_framed $uid $miv] == 0} {
	set first $n
	set top $uid
	set nv [WPCmd PEMailbox nextvector $first $ppg [list indexparts indexcolor status statusbits ]]
	set miv [lindex $nv 0]
	set charset [lindex $nv 1]
      }

      #set uid 0   ;# no longer "last viewed"
      #catch {WPCmd PEInfo unset uid}
    }

    # remember for next time
    catch {
      WPCmd PEInfo set top $top
      WPCmd PEInfo set sort $sort
      WPCmd PEInfo set rev $rev
    }
  }

  if {[llength $miv] == 0
      || (!([info exists charset] && [string length $charset])
	  && ([catch {WPCmd PEConfig varget character-set} charset]
	      || [string length [set charset [lindex $charset 0]]] == 0
	      || [string compare [string tolower $charset] "us-ascii"] == 0))} {
    set charset "UTF-8"
  }

  catch {fconfigure stdout -encoding binary}

  # start writing page
  cgi_http_head {
    WPStdHttpHdrs "text/html; charset=$charset"
  }

  cgi_html {
    cgi_head {
      cgi_http_equiv Content-Type "text/html; charset=$charset"

      set onload "onLoad="
      set onunload "onUnload="
      if {[info exists _wp(exitonclose)]} {
	WPExitOnClose
	append onload "wpLoad();"
	append onunload "wpUnLoad();"
      }

      if {[info exists _wp(timing)]} {
	set onsubmit "onSubmit=return submitTimestamp()"
	cgi_script  type="text/javascript" language="JavaScript" {
	  cgi_puts "var loadtime = new Date();"
	  cgi_put  "function submitTimestamp(){"
	  cgi_put  " var now = new Date();"
	  cgi_put  " document.index.submitted.value = now.getTime();"
	  cgi_put  " return true;"
	  cgi_puts "}"
	  cgi_put  "function fini() {"
	  cgi_put  " var now = new Date();"
	  cgi_put  " var t_load = (now.getTime() - loadtime.getTime())/1000;"
	  if {$submitted} {
	    cgi_put  " var t_submit = (now.getTime() - $submitted)/1000;"
	    set rtt ", rtt = '+t_submit+', cumulative = '+(t_submit + t_load)"
	  } else {
	    set rtt "'"
	  }
	  cgi_put  " window.status = 'Page loaded in '+t_load+' seconds${rtt};"
	  cgi_puts "}"
	}
	append onload "fini();"
      } else {
	set onsubmit ""
      }

      set normalreload [cgi_buffer {WPHtmlHdrReload "$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=index"}]
      if {[info exists _wp(exitonclose)]} {
	cgi_script  type="text/javascript" language="JavaScript" {
	  cgi_put  "function indexReloadTimer(t){"
	  cgi_put  " reloadtimer = window.setInterval('wpLink(); window.location.replace(\\'[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=index&reload=1\\')', t * 1000);"
	  cgi_puts "}"
	}

	append onload "indexReloadTimer($_wp(refresh));"
	cgi_noscript {
	  cgi_puts $normalreload
	}
      } else {
	cgi_puts $normalreload
      }

      WPStyleSheets
      cgi_puts  "<style type='text/css'>"
      cgi_puts  ".marked { font-weight: bold ; color: black }"
      cgi_puts  ".marked0 { font-size: 7pt; font-family: arial, sans-serif ; font-weight: bold ; color: black ; background-color: gold ; padding: 1 ; border: 1px solid black }"
      cgi_puts  ".aggop     { font-family: arial, sans-serif ; font-size: 7pt }"
      cgi_puts  "A.aggop { color: white ; font-size: 7pt }"
      cgi_puts  ".navbutton { font-family: arial, sans-serif ; font-size: 7pt; overflow: visible; width: auto; padding: 0 2px; margin-right: 2px }"
      cgi_puts  ".itable { font-family: geneva, arial, sans-serif }"
      cgi_puts  ".gradient { background-image: url('[WPimg indexhdr]') ; background-repeat: repeat-x }"
      cgi_puts  ".icell { white-space: nowrap; padding-right: 8px }"
      cgi_puts  ".icell0 { white-space: nowrap }"
      cgi_puts "</style>"

      cgi_script  type="text/javascript" language="JavaScript" {
	cgi_put  "function flip(d){"
	cgi_put  " var f = document.index;"
	cgi_put  " if(f && document.implementation){"
	cgi_put  "  var e = document.createElement('input');"
	cgi_puts "  var ver = navigator.appVersion;"
	cgi_put  "  if(!((ver.indexOf('MSIE')+1) && (ver.indexOf('Macintosh')+1))) e.type = 'hidden';"
	cgi_put  "  e.name = 'bod_'+d;"
	cgi_put  "  e.value = 1;"
	cgi_put  "  f.appendChild(e);"
	cgi_put  "  f.submit();"
	cgi_put  "  return false;"
	cgi_put  " }"
	cgi_put  " return true;"
	cgi_puts "}"

	cgi_put  "function view(u) {"
	cgi_put  " var f = document.index;"
	cgi_put  " f.target = '$vtarget';"
	cgi_put  " f.page.value = 'fr_view';"
	cgi_put  " f.uid.value = u;"
	cgi_puts " f.submit();"
	cgi_puts " return false;"
	cgi_puts "}"

	cgi_put  "function toggleMarks(elobj){"
	cgi_put  " var i, ckd = 1;"
	cgi_put  " for(i = 0; i < document.index.uidList.length; i++)"
	cgi_put  "  if(!document.index.uidList\[i\].checked){"
	cgi_put  "   ckd = 0;"
	cgi_put  "   break;"
	cgi_put  "  }"
	cgi_put  " for(i = 0; i < document.index.uidList.length; i++) document.index.uidList\[i\].checked = !ckd;"
	cgi_put  " elobj.src = (ckd) ? '[WPimg markall3]' : '[WPimg marknone3]';"
	cgi_put  " return false;"
	cgi_puts "}"
      }

      if {$_wp(keybindings)} {
	set kequiv {
	  {{?} {top.location = 'wp.tcl?page=help'}}
	  {{l} {top.location = 'wp.tcl?page=folders'}}
	  {{a} {top.location = 'wp.tcl?page=addrbook'}}
	  {{n} {if(flip('next')) location = 'wp.tcl?page=body&bod_next=1'}}
	  {{p} {if(flip('prev')) location = 'wp.tcl?page=body&bod_prev=1'}}
	  {{ } {if(flip('next')) location = 'wp.tcl?page=body&bod_next=1'}}
	  {{-} {if(flip('prev')) location = 'wp.tcl?page=body&bod_prev=1'}}
	  {{;} {if(document.index.select) document.index.select.click(); else document.index.aggon.click()}}
	  {{z} {if(document.index.zoom) document.index.zoom.click()}}
	}

	lappend kequiv [list {c} "top.location = 'wp.tcl?page=compose&oncancel=main.tcl&cid=[WPCmd PEInfo key]'"]

	if {$aggops && ($aggtabstate & $aggbar)} {
	  set exclusions document.index.f_name
	} else {
	  set exclusions ""
	}

	append onload [WPTFKeyEquiv $kequiv $exclusions]
      }
    }

    cgi_body bgcolor=$_wp(bordercolor) background=[file join $_wp(imagepath) logo $_wp(logodir) back.gif] "style=\"background-repeat: repeat-x\"" $onload $onunload {

      catch {WPCmd set help_context index}

      # check for status msg updates
      foreach s [WPStatusMsgs] {
	lappend newmail [list $s]
      }

      if {[llength $newmail]} {
	WPTFStatusTable $newmail 1 "style=\"padding: 6px 0; color: white\""

	if {!$reload} {
	  WPCmd PEMailbox newmailreset
	}
      }

      if {$ppg > 50} {
	set postmethod post
	set enctype "multipart/form-data"
      } else {
	set postmethod get
	set enctype "application/x-www-form-urlencoded"
      }

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=$postmethod enctype=$enctype name=index $onsubmit target=body {

	# context line
	cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" class="context" {
	  if {[llength $newmail]} {
	    cgi_table_row {
	      cgi_table_data height=1 bgcolor="#000000" width="100%" colspan=8 {
		cgi_put [cgi_img [WPimg dot2] border=0 height=1]
	      }
	    }
	  }

	  cgi_table_row bgcolor="#999999" {
	    if {$zoomed} {
	      if {[catch {WPCmd PEMailbox messagecount after [lindex [lindex $miv end] 0]} messagesremaining]} {
		unset messagesremaining
	      }

	      set z "[cgi_span class="marked" marked] "
	      set t $zoomed
	    } else {
	      set messagesremaining [expr {$messagecount - ($first + $ppg - 1)}]
	      set t $messagecount
	      set z ""
	    }

	    if {[catch {WPCmd PEMailbox messagecount before [lindex [lindex $miv 0] 0]} messagesbefore] == 0 && $messagesbefore > 0} {
	      if {$messagesbefore > $ppg} {
		set b $ppg
	      } else {
		set b $messagesbefore
	      }

	      switch $b {
		1 {
		  set c One
		  set p ""
		}
		default {
		  set c $b
		  set p s
		}
	      }

	      set prevtext " [cgi_unbreakable_string "($c on [cgi_url previous wp.tcl?page=body&bod_prev=1 "onClick=return flip('prev')"] page)"]"
	    } else {
	      set messagesbefore 0
	      set prevtext ""
	    }

	    if {$messagesbefore <= 0 && $messagesremaining <= 0} {
	      switch $t {
		0 {
		  set counttext "[cgi_bold No] ${z}messages in folder"
		}
		1 {
		  set counttext "[cgi_bold Only] ${z}message in folder"
		}
		2 {
		  set counttext "[cgi_bold Both] ${z}messages in folder"
		}
		default {
		  set counttext "[cgi_bold All] $t ${z}messages in folder"
		}
	      }
	    } elseif {$zoomed} {
	      if {$messagesbefore > 0} {
		if {$messagesremaining > 0} {
		  set counttext "[expr {$messagesbefore + 1}] thru [expr {$zoomed - $messagesremaining}] of $zoomed [cgi_span class="marked" marked] messages"
		} else {
		  if {[set p [llength $miv]] == 1} {
		    set lasttext ""
		  } else {
		    set lasttext " $p"
		  }

		  set counttext "[cgi_bold Last]${lasttext} of $zoomed [cgi_span class="marked" marked] messages"
		}
	      } else {
		set counttext "[cgi_bold First] $ppg of $zoomed [cgi_span class="marked" marked] messages"
	      }
	    } else {
	      set n [WPcomma $messagecount]

	      if {[lindex [lindex $miv 0] 0] == 1} {
		set counttext "[cgi_bold First] $ppg of $n messages"
	      } else {
		set l [lindex [lindex $miv end] 0]
		if {$l == $messagecount} {
		  if {[set p [llength $miv]] == 1} {
		    set lasttext ""
		  } else {
		    set lasttext " $p"
		  }

		  set counttext "[cgi_bold Last]${lasttext} of $n messages"
		} else {
		  set counttext "Message [WPcomma $first] - [WPcomma $l] of $n"
		}
	      }
	    }

	    cgi_table_data align=left class="context" "style=\"padding-left: 4\"" {
	      cgi_unbreakable {
		cgi_put "${counttext}[cgi_breakable]${prevtext}"
	      }
	    }

	    cgi_table_data align=right {
	      cgi_unbreakable {
		if {[WPCmd PEInfo feature expunge-without-confirm-everywhere]
		    || ([WPCmd PEInfo feature expunge-without-confirm]
			&& [string compare [string tolower [WPCmd PEMailbox mailboxname]] inbox] == 0)} {
		  set butname expunge
		  cgi_text "emptyit=1" type=hidden notab
		} else {
		  set butname queryexpunge
		}

		if {$aggops} {
		  cgi_submit_button "mark=Mark All in Folder" class="navbutton"
		  cgi_submit_button "unmark=Unmark All" class="navbutton"

		  cgi_put [cgi_breakable]
		  cgi_submit_button "setflag=Delete" class="navbutton"
		  cgi_submit_button "setflag=Undelete" class="navbutton"
		  if {([info exists _wp(spamaddr)] && [string length $_wp(spamaddr)])
		      || ([info exists _wp(spamfolder)] && [string length $_wp(spamfolder)])} {
		    cgi_submit_button "spamit=Report Spam" class="navbutton" "style=\" color: white; background-color: black\""
		  }
		}

		cgi_submit_button "${butname}=Expunge" class="navbutton"
	      }
	    }
	  }
	}

	cgi_text "page=index" type=hidden notab
	cgi_text "cid=$cid" type=hidden notab
	cgi_text "first=$first" type=hidden notab
	cgi_text "uid=0" type=hidden notab
	cgi_text "top=$top" type=hidden notab
	cgi_text "frestore=1" type=hidden notab
	cgi_text "submitted=0" type=hidden notab

	if {$aggops} {
	  if {[llength $miv]} {
	    foreach v $miv {
	      lappend uv [lindex $v 1]
	    }

	    set uidpage [join $uv ","]
	  } else {
	    set uidpage ""
	  }

	  cgi_text "uidpage=${uidpage}" type=hidden notab

	  if {$aggtabstate & $aggbar} {
	    # commands
	    cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" class=ops {
	      cgi_table_row {
		cgi_table_data height=1 "bgcolor=#000000" colspan=12 align=left {
		  cgi_put [cgi_img [WPimg dot2] height=1]
		}
	      }
	      cgi_table_row {
		cgi_put "<td height=100% width=11px align=left valign=bottom background=[WPimg barclose_mid]><input type=image name=\"aggoff\" src=[WPimg barclose] border=0 alt=\"Hide Message Commands\"></td>"

		cgi_table_data align=center valign=middle nowrap class=aggop width=30% {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"color: white\">Search and Mark</legend>"
		  cgi_submit_button "select=${selectverb}" class=aggop "style=\"vertical-align: middle; margin-right: 4\""

		  if {$zoomed} {
		    cgi_submit_button "zoom=Show All Messages" class=aggop "style=\"vertical-align: middle\""
		  } else {
		    cgi_submit_button "zoom=Show Only Marked" class=aggop "style=\"vertical-align: middle\""
		  }
		  cgi_puts "</fieldset>"
		}

		cgi_table_data align=center valign=middle nowrap class=aggop width=30% {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"color: white\">Message Status</legend>"
		  cgi_submit_button setflag=Set class=aggop "style=\"vertical-align:middle\""
		  cgi_put "[cgi_nbspace]status to "
		  cgi_select flags class=aggop "style=\"vertical-align:middle\"" {
		    #cgi_option Deleted value=delete
		    #cgi_option Undeleted value=undeleted
		    cgi_option New value=new selected
		    cgi_option Read value=read
		    cgi_option Important value=important
		    cgi_option Unimportant value=unimportant
		    cgi_option Answered value=answered
		    cgi_option Unanswered value=unanswered
		  }

		  cgi_puts "</fieldset>"
		}

		cgi_table_data align=center valign=middle class=aggop width=40% {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"color: white\">Save Messages</legend>"
		  # * * * * Save * * * *

		  cgi_submit_button "save=Save" class=aggop "style=\"vertical-align:middle\""
		  cgi_put "[cgi_nbspace]to "

		  set savedef [WPTFSaveDefault $uid]

		  cgi_text f_colid=[lindex $savedef 0] type=hidden notab

		  cgi_select f_name class=aggop "style=\"vertical-align:middle\"" "onchange=document.index.save.click(); return false;" {
		    foreach {oname oval} [WPTFGetSaveCache [lindex $savedef 1]] {
		      cgi_option $oname value=$oval
		    }
		  }

		  #cgi_put "[cgi_nbspace]"
		  #cgi_submit_button "save=OK" class=aggop "style=\"vertical-align:middle;margin-right:2\""
		  cgi_puts "</fieldset>"
		}
	      }
	      cgi_table_row {
		cgi_table_data height=1 "bgcolor=#000000" colspan=12 {
		  cgi_put [cgi_img [WPimg dot2] height=1]
		}
	      }
	    }
	  } elseif {$aggtabstate & $sortbar} {
	    # aggop bar
	    cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" class=ops {
	      cgi_table_row {
		cgi_table_data height=1 "bgcolor=#000000" colspan=12 align=left {
		  cgi_put [cgi_img [WPimg dot2] height=1]
		}
	      }
	      cgi_table_row {
		cgi_table_data {
		  cgi_image_button aggon=[WPimg baropen] border=0 alt=\"Show Search/Save Commands\"
		}
	      }

	      cgi_table_row {
		cgi_table_data height=1 "bgcolor=#000000" colspan=12 {
		  cgi_put [cgi_img [WPimg dot2] height=1]
		}
	      }
	    }
	  }
	}

	cgi_table cellspacing=0 cellpadding=0 width="100%" class="itable" {
	  if {$messagecount < 1} {
	    # special case for mailbox with no messges
	    cgi_table_row {
	      cgi_table_data class=body valign=middle height=[expr {($ppg + 1) * $indexheight}] {
		cgi_center {
		  cgi_puts [cgi_font size=+2 "\[Folder \"[WPCmd PEMailbox mailboxname]\" contains no messages\]"]
		}
	      }
	    }
	  } else {

	    # get the desired index item order and spacing
	    set iformat [WPCmd PEMailbox indexformat]

	    set colspan [expr {[llength $iformat] + 1}]

	    # setup per-column layout parameters
	    foreach fmt $iformat {

	      if {[info exists oncethru]} {
		if {![info exists firstcell]} {
		  set class icell0
		  set firstcell 1
		} else {
		  set class icell
		}

		append layout " $class "
	      }

	      set oncethru 1

	      switch -regexp -- [lindex $fmt 0] {
		[Ss]tatus {
		  # fixed with "envelope" icon
		  if {[string length [lindex $fmt 2]]} {
		    set width "1%"
		  } else {
		    set width "42px"
		  }
		}
		default {
		  # proportion computed by pith (may be user specified)
		  if {[regexp {[0123456789]+[%]} [lindex $fmt 1]]} {
		    set width [lindex $fmt 1]
		  } else {
		    set width "1%"
		  }
		}
	      }

	      append layout "$width"
	    }

	    append layout " icell0"

	    #set doscrollbar [expr {($zoomed && $zoomed > $ppg) || (!$zoomed && $messagecount > $ppg)}]
	    set linenum 1

	    # paint the index line column headers
	    cgi_table_row class=\"gradient\" {
	      if {($aggtabstate & $sortbar) == 0} {
		if {!([info exists headertab] && [string length $headertab])} {
		  cgi_table_data class=indexhdr align=left width=48px background=[WPimg baropen_mid] {
		    if {$aggops && ($aggtabstate & $aggbar) == 0} {
		      cgi_image_button aggon=[WPimg baropen] border=0 alt=\"Show Search/Save Commands\"
		    }

		    cgi_image_button hdron=[WPimg baropen] border=0 alt=\"Show List Headers\"
		  }

		  foreach fmt $iformat {width class} $layout {
		    cgi_table_data class=indexhdr align=left width=$width background=[WPimg baropen_mid] {
		      cgi_put [cgi_img [WPimg dot2] height=1]
		    }
		  }
		}
	      } else {
		if {$aggops} {
		  cgi_table_data height=100% align=left valign=bottom class=indexhdr width=\"40px\" {
		    cgi_table width=\"100%\" cellpadding=0 cellspacing=0 border=0 {
		      cgi_table_row {
			cgi_table_data align=left valign=bottom {
			  cgi_image_button hdroff=[WPimg barclose] border=0 "alt=\"Hide List Headers\""
			}
			cgi_table_data align=center valign=middle "style=\"padding-right: 14px\"" {
			  set marked 1
			  foreach v $miv {
			    set u [lindex $v 1]
			    if {$u == 0 || ![WPCmd PEMessage $u select]} {
			      set marked 0
			      break
			    }
			  }

			  if {$marked} {
			    cgi_image_button unmark=[WPimg marknone3] border=0 alt=\"Unmark Messages on Page\" "onClick=return toggleMarks(this);"
			  } else {
			    cgi_image_button mark=[WPimg markall3] border=0 alt=\"Mark Messages on Page\" "onClick=return toggleMarks(this);"
			  }
			}
		      }
		    }
		  }
		} else {
		  cgi_td class=indexhdr [cgi_nbspace]
		}

		set sorts [string tolower [WPCmd PEMailbox sortstyles]]

		foreach fmt $iformat {width class} $layout {
		  set title [lindex $fmt 0]
		  catch {unset sortlist}

		  if {[lsearch -exact $sorts [string tolower $title]] >= 0} {
		    if {[string compare [string tolower [lindex $currentsort 0]] [string tolower $title]] == 0} {
		      lappend sortlist [list indexsort [sortname $title 1]]
		    } else {
		      lappend sortlist [list indexhdr [cgi_url [sortname $title] wp.tcl?page=index&sort[string tolower $title]=1 class=indexhdr title='Sort by $title' target=body]]
		    }

		    # special subject sort handling
		    switch -regexp -- $title {
		      [Ss]ubject { lappend extrasort OrderedSubj ; lappend extrasort Thread }
		      [Ff]rom { lappend extrasort To }
		      x[Dd]ate { lappend extrasort Arrival }
		    }

		    if {[info exists extrasort]} {
		      foreach s $extrasort {
			lappend sortlist [list indexhdr [cgi_bold "|"]]
			# append text [cgi_nbspace][cgi_bold "|"][cgi_nbspace]
			if {[string compare [string tolower [lindex $currentsort 0]] [string tolower $s]] == 0} {
			  if {[string compare [string tolower $s] thread] == 0} {
			    set threadsort 1
			  }
			  lappend sortlist [list indexsort [sortname $s 1]]
			} else {
			  lappend sortlist [list indexhdr [cgi_url [sortname $s] wp.tcl?page=index&sort[string tolower $s]=1 class=indexhdr title='Sort by $s' target=body]]
			}
		      }

		      # clear for next time
		      unset extrasort
		    }
		  } else {
		    lappend sortlist [list indexhdr [sortname $title]]
		  }

		  cgi_table_data class=indexhdr valign=middle class=$class width="$width" {
		    cgi_table border=0 cellspacing=0 cellpadding=2 {
		      cgi_table_row {
			foreach s $sortlist {
			  cgi_table_data class=[lindex $s 0] nowrap {
			    cgi_puts [lindex $s 1]
			  }
			}
		      }
		    }
		  }
		}

		if {[info exists use_plus_minus_to_grow_shrink]} {
		  # grow/shrink controls
		  cgi_table_data class=indexhdr {
		    cgi_image_button growpage=[WPimg plus2] height=12 border=0 alt="Grow"
		    cgi_image_button shrinkpage=[WPimg minus2] height=12 border=0 alt="Shrink"
		  }
		}
	      }
	    }

	    if {$zoomed} {
	      cgi_table_row {
		cgi_table_data class=marked0 colspan=$colspan align=center {
		  cgi_puts "Unmarked Messages are NOT Shown - Click [cgi_url "Show All Messages" wp.tcl?page=body&zoom=0] to View All"
		  #cgi_puts "Unmarked Messages Are Excluded From View"
		}
	      }
	    }

	    # get ready to map thread images
	    if {[string compare [string tolower [lindex $currentsort 0]] thread] == 0} {
	      set barblank [WPThreadImageLink barblank $indexheight]
	      set barvert [WPThreadImageLink barvert $indexheight]
	      if {[lindex $currentsort 1]} {
		set barmsg [WPThreadImageLink ibarmsg $indexheight]
	      } else {
		set barmsg [WPThreadImageLink barmsg $indexheight]
	      }

	      if {[lindex $currentsort 1]} {
		set barvertmsg [WPThreadImageLink ibarvertmsg $indexheight]
	      } else {
		set barvertmsg [WPThreadImageLink barvertmsg $indexheight]
	      }
	    }

	    foreach v $miv {
	      set n [lindex $v 0]
	      set u [lindex $v 1]
	      set msg [lindex [lindex $v 2] 0]
	      set linecolor [lindex [lindex $v 2] 1]
	      set stat [lindex [lindex $v 2] 2]
	      set statbits [lindex [lindex $v 2] 3]

	      set class [lineclass [incr linenum]]

	      if {$n > $messagecount} {
		break
	      }

	      if {[llength $linecolor] == 2 && [string compare [lindex $linecolor 0] [lindex $linecolor 1]]} {
		set style "color: #[lindex $linecolor 0] ; background-color: #[lindex $linecolor 1]"
	      } else {
		set style ""
	      }

	      cgi_table_row class=$class "style=\"$style\"" {
		if {$u == 0} {
		  cgi_table_data colspan=$colspan height=$indexheight {
		    cgi_put "Data for message $n no longer available"
		  }
		} else {
		  if {$aggops} {
		    cgi_table_data valign=middle align=center height=$indexheight {
		      if {[WPCmd PEMessage $u select]} {
			set checked checked
		      } else {
			set checked ""
		      }

		      cgi_checkbox "uidList=$u" $checked class=$class id=cb$u "style=\"$style; margin-left: 16\""
		    }
		  } else {
		    cgi_td height=$indexheight width=2% [cgi_nbspace]
		  }

		  set deleted [string index $stat 0]
		  set recent [expr {[string range $stat 0 2] == "010"}]

		  foreach part $msg fmt $iformat {width class} $layout {

		    set align ""
		    set onclick ""

		    switch -exact -- [lindex $fmt 0] {
		      Subject {
			set parttext ""
			set leading ""
			foreach p $part {
			  switch -- [lindex $p 2] {
			    threadinfo {
			      append leading [lindex $p 0]
			      regsub -all {[ ][ ]} $leading $barblank leading
			      regsub -all {[|][-]} $leading $barvertmsg leading
			      regsub -all {[\\][-]} $leading $barmsg leading
			      regsub -all {[|]} $leading $barvert leading
			    }
			    xthreadinfo {
			      set t [lindex $p 0]
			      append leading "$t"
			      for {set i 0} {$i < [string length $t]} {incr i} {
				switch -- [string index $t $i] {
				  " " {
				    append leading [WPThreadImageLink barblank $indexheight]
				  }
				  ">" {
				  }
				  "-" {
				    if {[lindex $currentsort 1]} {
				      append leading [WPThreadImageLink ibarmsg $indexheight]
				    } else {
				      append leading [WPThreadImageLink barmsg $indexheight]
				    }
				  }
				  "|" {
				    append leading [WPThreadImageLink barvert $indexheight]
				  }
				}
			      }
			    }
			    default {
			      append parttext [index_part_color [index_quote [lindex $p 0]] [lindex $p 1]]
			    }
			  }
			}

			if {[string length [set label $parttext]] == 0} {
			  set label {[Empty Subject]}
			}

			set text [cgi_url $label "fr_view.tcl?&uid=$u&c=[string range $cid 0 5]" target=$vtarget "onClick=return view($u)"]

			if {![info exists do_status_icons] && $deleted} {
			  set text [cgi_span "style=text-decoration: line-through" $text]
			}

			set text [cgi_buffer {
			  cgi_division "style=\"height: $indexheight; overflow: hidden;\"" {
			    cgi_put "${leading}${text}"
			  }
			}]
		      }
		      Status {
			if {[string length [lindex $fmt 2]]} {
			  set text ""
			  foreach i $part {
			    regsub -all { } [lindex $i 0] {\&nbsp;} statstr
			    if {[llength [lindex $i 1]]} {
			      append text [cgi_span "style=background-color: #[lindex [lindex $i 1] 1]; color: #[lindex [lindex $i 1] 0]" $statstr]
			    } else {
			      append text $statstr
			    }
			  }

			  set text [cgi_buffer {
			    cgi_division "style=\"font-family: monospace\"" {
			      cgi_put $text
			    }
			  }]
			} elseif {[info exists do_status_icons]} {
			  set text [WPStatusImg $u]
			} else {
			  set text [lindex [WPStatusIcon $u gif $statbits] 2]
			  set align align=center
			}

			if {$flagcmd} {
			  set text [cgi_url $text fr_flags.tcl?uid=$u target=body]
			}
		      }
		      Size {
			set text [index_part_color [index_quote [lindex [lindex $part 0] 0]] [lindex $part 1]]
			set class isize
			set onclick "onclick=\"flipCheck('cb$u')\""
		      }
		      Number {
			set text [index_part_color [index_quote [WPcomma [string trim [lindex [lindex $part 0] 0]]]] [lindex $part 1]]
			set onclick "onclick=\"flipCheck('cb$u')\""
		      }
		      From -
		      To {
			set t [index_quote [lindex [lindex $part 0] 0]]
			if {$recent} {
			  set t [cgi_bold $t]
			}

			set text [cgi_buffer {
			  cgi_division "style=\"height: $indexheight; overflow: hidden;\"" {
			    cgi_put [index_part_color $t [lindex $part 1]]
			  }
			}]

			set onclick "onclick=\"flipCheck('cb$u')\""
		      }
		      default {
			set text [index_part_color [index_quote [lindex [lindex $part 0] 0]] [lindex $part 1]]
			set onclick "onclick=\"flipCheck('cb$u')\""
		      }
		    }

		    if {![info exists text]} {
		      set text doh
		    }

		    cgi_td $align class="$class" $onclick "$text"
		  }

		  if {[info exists use_plus_minus_to_grow_shrink]} {
		    cgi_td [cgi_nbspace]
		  }
		}
	      }
	    }

	    for {} {$linenum <= $ppg} {incr linenum} {
	      cgi_table_row class="[lineclass [expr $linenum + 1]]" {
		cgi_table_data colspan=$colspan height=$indexheight {
		  cgi_puts [cgi_nbspace]
		}
	      }
	    }
	  }

	  if {[info exists use_bottom_text_to_grow_shrink]} {
	    cgi_table_row height=1 {
	      cgi_table_data bgcolor=#000000 colspan=$colspan {
		cgi_put [cgi_img [WPimg blackdot] height=1]
	      }
	    }

	    cgi_table_row {
	      cgi_table_data align=center valign=middle colspan=$colspan class=indexhdr {
		cgi_put "[WPcomma $ppg] messages are in the list above.  Press either "
		cgi_submit_button growpage=$growverb class=indexhdr "style=\"vertical-align:middle\""
		cgi_put " or "
		cgi_submit_button shrinkpage=$shrinkverb class=indexhdr "style=\"vertical-align:middle\""
		cgi_put " to change this by "
		cgi_select grownum class=indexhdr "style=\"vertical-align:middle\"" {
		  set growsizes {1 5 10 25 50}

		  if {[catch {WPCmd PEInfo set grownum} lastsize]} {
		    set lastsize 0
		  }

		  foreach size $growsizes {
		    if {$size == $lastsize} {
		      set sel selected
		    } else {
		      set sel ""
		    }

		    cgi_option $size value=$size $sel
		  }
		}
		cgi_put "."
	      }
	    }
	  }
	}
      }

      cgi_table width=100% cellpadding=0 cellspacing=0 border=0 {
	cgi_table_row {
	  cgi_table_data align=left class=context {
	    if {[info exists messagesremaining] && $messagesremaining > 0} {
	      if {$messagesremaining > $ppg} {
		set moretext ", $ppg"
	      } else {
		set moretext ""
	      }

	      if {$zoomed} {
		set marked " [cgi_bold marked]"
	      } else {
		set marked ""
	      }

	      set nexttext " ([WPcomma $messagesremaining] more${marked} message[WPplural $messagesremaining]${moretext} on [cgi_url next wp.tcl?page=body&bod_next=1 "onClick=return flip('next')"] page)"
	    } else {
	      set nexttext ""
	    }

	    cgi_puts "[cgi_nbspace]${counttext}${nexttext}"
	  }
	  cgi_table_data align=right class=context {
	    cgi_put "Powered by [cgi_url Alpine "http://www.washington.edu/alpine/" target="_blank"] - [cgi_copyright] 2007 University of Washington"
	    if {[info exists _wp(ui2dir)]} {
	      cgi_puts " - [cgi_url "Standard Version " "$_wp(serverpath)/$_wp(appdir)/$_wp(ui2dir)/browse" target=_top]"
	    }
	  }
	}
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

	cgi_puts [cgi_font size=-2 "style=font-family:arial;font-weight:bold" "\[time: ${scripttime}${servlettime}\]"]
      }
    }
  }
}

#!./tclsh
# $Id: newlist.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  newlist.tcl
#
#  Purpose:  CGI script that generates a page displaying a message
#            list of the indicated folder. 
#
#  Input:    PATH_INFO: [/<col_number>]/<folder_name>[/<uid_of_first_msg>
#            along with possible search parameters:
set newlist_args {
  {op		{}	""}
  {df		{}	""}
  {uid		{}	""}
  {type		{}	""}
  {zoom		{}	""}
  {page		{}	""}
  {criteria	{}	""}
  {scope	{}	"new"}
}

# inherit global config
source ./alpine.tcl
source ./common.tcl
source ./foldercache.tcl

# default newlist state
set c 0
set f "INBOX"

# TEST
proc cgi_suffix {args} {
  return ""
}

set dmsgs ""
proc dm {s} {
  global dmsgs

  lappend dmsgs $s
}

proc focusOnResult {_focused} {
  upvar 1 $_focused focused

  if {[catch {WPCmd PEMailbox focus 1} focused]} {
    WPCmd PEInfo statmsg "Cannot focus: $focused"
    set focused 0
  } else {
    WPCmd PEInfo statmsg "Displaying $focused search results"
  }
}

# grok PATH_INFO for collection, 'c', and folder 'f'
if {[info exists env(PATH_INFO)] && [string length $env(PATH_INFO)]} {
  if {[regexp {^/([0-9]+)/(.*)$} $env(PATH_INFO) dummy c f]} {
    # Import data validate it and get session id
    if {[catch {WPGetInputAndID sessid} result]} {
      set harderr "Cannot init session: $result"
    } else {
      # grok parameters
      foreach item $newlist_args {
	if {[catch {eval WPImport $item} result]} {
	  set harderr "Cannot read input: $result"
	  break;
	}
      }
    }
  } else {
    set harderr "BOTCH: invalid folder: $env(PATH_INFO)"
  }
} else {
  set harderr "BOTCH: No folder specified"
}

cgi_puts "Content-type: text/html; charset=\"UTF-8\"\n"

if {[info exists harderr]} {
  cgi_puts "<b>ERROR: $harderr</b>"
  exit
}

set defc [WPCmd PEFolder defaultcollection]
set focused [WPCmd PEMailbox focus]
set mc [WPCmd PEMailbox messagecount]

# set uid to "current" message?
if {0 == [catch {WPCmd PEMailbox current} cm]} {
  set n [lindex $cm 0]
  set u [lindex $cm 1]
} else {
  if {$mc > 0} {
    WPCmd PEInfo statmsg "BOTCH: No current message"
  }

  # non given, set to first message in folder
  set n 1
  if {[catch {
    set n [WPCmd PEMailbox first]
    set u [WPCmd PEMailbox uid $n]
  } result]} {
    if {$mc > 0} {
      WPCmd PEInfo statmsg "No first message: $result"
    }

    set n 0
    set u 0
  }
}

# lines per page
if {[catch {WPCmd PEInfo indexlines} ppg] || $ppg <= 0} {
  set ppg $_wp(indexlines)
}

# deal with page change
if {$mc > 0} {
  switch -regexp -- $op {
    ^next$ {
      set n [WPCmd PEMailbox next $n $ppg]

      if {[catch {WPCmd PEMailbox uid $n} u]} {
	set n [WPCmd PEMailbox first]
	set u [WPCmd PEMailbox uid $n]
      }
    }
    ^prev$ {
      if {$ppg >= $n} {
	set n [WPCmd PEMailbox first]
      } else {
	set n [WPCmd PEMailbox next $n -$ppg]
      }

      if {[catch {WPCmd PEMailbox uid $n} u]} {
	set n [WPCmd PEMailbox first]
	set u 0
      }
    }
    ^first$ {
      set n [WPCmd PEMailbox first]
      if {[catch {WPCmd PEMailbox uid $n} u]} {
	set n 1
	set u 0
      }
    }
    ^last$ {
      set n [WPCmd PEMailbox last]
      if {[catch {WPCmd PEMailbox uid $n} u]} {
	set n 1
	set u 0
      }
    }
    ^delete$ {
      if {[catch {WPCmd PEMailbox apply flag ton del} result]} {
	WPCmd PEInfo statmsg "Cannot Delete: $result"
      } else {
	WPCmd PEInfo statmsg "$result message[WPplural $result] deleted"
      }
    }
    ^trash$ {
      if {[catch {WPCmd PEFolder empty $c [wpLiteralFolder $c $f] selected} result]} {
	WPCmd PEInfo statmsg "Cannot Remove: $result"
      } else {
	WPCmd PEInfo statmsg "$result message[WPplural $result] deleted forever"
	if {0 == [WPCmd PEMailbox messagecount]} {
	  set n 0
	  set u 0
	}
      }
    }
    ^trashall$ {
      if {[catch {WPCmd PEFolder empty $c [wpLiteralFolder $c $f] all} result]} {
	WPCmd PEInfo statmsg "Cannot Remove: $result"
      } else {
	WPCmd PEInfo statmsg "$result message[WPplural $result] deleted forever"
	set n 0
	set u 0
      }
    }
    ^spam$ {
      set numspam [WPCmd PEMailbox selected]
      if {$numspam > 0} {
	# aggregate save 
	if {[info exists _wp(spamsubj)] && [string length $_wp(spamsubj)]} {
	  set spamsubj $_wp(spamsubj)
	} else {
	  set spamsubj "Spam Report"
	}

	# aggregate delete
	if {[info exists _wp(spamfolder)] && [string length $_wp(spamfolder)]
	    && [catch {
	      if {[WPCmd PEFolder exists $defc $_wp(spamfolder)] == 0} {
		WPCmd PEFolder create $defc $_wp(spamfolder)
	      }

	      WPCmd PEMailbox apply save $defc $_wp(spamfolder)
	    } result]} {
	  WPCmd PEInfo statmsg "Error Reporting Spam: $result"
	} elseif {[info exists _wp(spamaddr)] && [string length $_wp(spamaddr)]
		  && [catch {WPCmd PEMailbox apply spam $_wp(spamaddr) $spamsubj} reason]} {
	  WPCmd PEInfo statmsg "Error Sending Spam Notice: $reason"
	} elseif {[catch {WPCmd PEMailbox apply delete} reason]} {
	  WPCmd PEInfo statmsg "Error marking Spam Deleted: $reason"
	} else {
	  WPCmd PEInfo statmsg "$numspam spam message[WPplural $numspam] reported"
	}
      }
    }
    ^copy$ {
      if {[string length $df] && [regexp {^([0-9]+)/(.*)$} $df dummy dfc dfn] && [string length $dfn]} {
	if {[catch {WPCmd PEMailbox apply copy $dfc $dfn} result]} {
	  WPCmd PEInfo statmsg "Cannot copy messages: $result"
	} else {
	  if {$dfc == $defc
	      && !(([info exists _wp(spamfolder)] && 0 == [string compare $f $_wp(spamfolder)])
		   || 0 == [string compare $f Trash])} {
	    addSaveCache $dfn
	    set savecachechange $dfn
	  }

	  WPCmd PEInfo statmsg "Copied $result message[WPplural $result] to $dfn"
	}
      } else {
	WPCmd PEInfo statmsg "Cannot copy to $df"
      }
    }
    ^move$ {
      if {[string length $df] && [regexp {^([0-9]+)/(.*)$} $df dummy dfc dfn] && [string length $dfn]} {
	if {[catch {WPCmd PEMailbox apply move $dfc $dfn} result]} {
	  WPCmd PEInfo statmsg "Move Failure: $result"
	} else {
	  # if needed, empty what was moved
	  if {$c == [WPCmd PEFolder defaultcollection]
	      && (([info exists _wp(spamfolder)] && 0 == [string compare $f $_wp(spamfolder)])
		  || 0 == [string compare $f Trash])
	      && [catch {WPCmd PEFolder empty $c $f selected} result]} {
	    WPCmd PEInfo statmsg "Move Failure: $result"
	  } else {
	    if {$dfc == $defc
		&& !(([info exists _wp(spamfolder)] && 0 == [string compare $f $_wp(spamfolder)])
		     || 0 == [string compare $f Trash])} {
	      addSaveCache $dfn
	      set savecachechange $dfn
	    }

	    WPCmd PEInfo statmsg "Moved $result message[WPplural $result] to $dfn"
	  }
	}
      } else {
	WPCmd PEInfo statmsg "Cannot move: to $df"
      }
    }
    ^movemsg$ {
      if {[regexp {^[0-9]+$} $uid] && $uid > 0 && [string length $df] && [regexp {^([0-9]+)/(.*)$} $df dummy dfc dfn] && [string length $dfn]} {
	if {[catch {
	  # destination Trash? just delete and let regular delete process move it
	  if {$dfc == $defc && 0 == [string compare Trash $dfn]} {
	    WPCmd PEMessage $uid flag deleted 1
	  } else {
	    WPCmd PEMessage $uid move $dfc $dfn
	  }

	  # source is trash/junk, remove explicitly
	  if {$c == $defc
	      && (([info exists _wp(spamfolder)] && 0 == [string compare $f $_wp(spamfolder)])
		  || 0 == [string compare $f Trash])} {
	    WPCmd PEFolder empty $c $f $uid
	  }

	  WPCmd PEInfo statmsg "Moved message to $dfn"
	} result]} {
	  WPCmd PEInfo statmsg "Move Failure: $result"
	}
      } else {
	WPCmd PEInfo statmsg "Cannot move: to $df"
      }
    }
    ^sort[A-Za-z]+$ {
      if {[regexp {^sort([[Rr]ev|)(.*)$} $op dummy rev sname]} {
	set sort [string tolower $sname]
	set rval [expr {[string length $rev] > 0}]
	if {[catch {WPCmd PEMailbox sort $sort $rval} cursort]} {
	  WPCmd PEInfo statmsg "Cannot set sort: $cursor"
	  set cursort [list nonsense 0]
	} else {
	  # store result 
	  WPCmd set sort [list $sort $rval]
	}
      } else {
	WPCmd PEInfo statmsg "Unrecognized Sort: $op"
      }
    }
    ^search$ {
      if {![regexp {broad|narrow} $scope]} {
	catch {WPCmd PEMailbox search none}
	set scope broad
      }

      switch -- $type {
	none {
	  WPCmd PEMailbox focus 0
	  WPCmd PEMailbox search none
	}
	any {
	  if {![string length $criteria]} {
	    WPCmd PEInfo statmsg "No search criteria provided"
	  } elseif {[catch {WPCmd PEMailbox search $scope text ton any $criteria} result]} {
	      WPCmd PEInfo statmsg "Search failed: $result"
	  } else {
	    if {$result == 0} {
	      WPCmd PEInfo statmsg "No messages matched your search"
	      if {0 == [string compare new $scope]} {
		WPCmd PEMailbox focus 0
		WPCmd PEMailbox search none
	      }
	    } else {
	      set n [WPCmd PEMailbox first]
	      if {[catch {WPCmd PEMailbox uid $n} u]} {
		set n 1
		set u 0
	      }

	      focusOnResult focused
	    }
	  }
	}
	compound {
	  if {![string length $criteria]} {
	    WPCmd PEInfo statmsg "No search criteria provided"
	  } elseif {[catch {WPCmd PEMailbox search $scope compound $criteria} result]} {
	    WPCmd PEInfo statmsg "Search failed: $result"
	  } else {
	    if {$result == 0} {
	      WPCmd PEInfo statmsg "No messages matched your search"
	      if {0 == [string compare new $scope]} {
		WPCmd PEMailbox focus 0
		WPCmd PEMailbox search none
	      }
	    } else {
	      set n [WPCmd PEMailbox first]
	      if {[catch {WPCmd PEMailbox uid $n} u]} {
		set n 1
		set u 0
	      }

	      focusOnResult focused
	    }
	  }
	}
	default {
	  WPCmd PEInfo statmsg "Unrecognized search: $type"
	}
      }
    }
    ^focus$ {
      focusOnResult focused
    }
    ^unfocus$ {
      if {[catch {WPCmd PEMailbox focus 0} result]} {
	WPCmd PEInfo statmsg "Cannot unfocus: $result"
      } elseif {$focused > 0} {
	WPCmd PEInfo statmsg "All messages displayed"
	set focused 0
      }
    }
    noop -
    ^$ {
    }
    default {
    }
  }
}

if {$focused} {
  set mc $focused
}

# page framing (note maybe changed by actions above)
wpInitPageFraming u n mc ppg pn pt

cgi_puts [WPCmd cgi_buffer "drawMessageList $c {$f} $n $ppg"]

cgi_puts "<script>"
cgi_put "updateBrowseLinksAndSuch(\{"
cgi_put "u:$u,selected:[WPCmd PEMailbox selected],"
cgi_put "unread:[WPCmd PEMailbox flagcount [list unseen undeleted]],"
cgi_put "page:$pn,pages:$pt,count:$mc,"
cgi_put "searched:[WPCmd PEMailbox searched],focused:$focused,"
cgi_put "sort:'[lindex [WPCmd PEMailbox sort] 0]'"
cgi_puts "\});"
if {0 == [string compare $page new]} {
  cgi_puts "showBrowseMenus();"
  cgi_puts "initMenus();"
  cgi_puts "initMorcButton('listMorcButton');"
  cgi_puts "initSelection();"
  wpSaveMenuJavascript "browse" $c $f [WPCmd PEFolder defaultcollection] morcInBrowseDone
  cgi_puts "setNewMailCheckInterval([WPCmd PEInfo inputtimeout],'newMessageList()');"
  cgi_puts "if(self.loadDDElements) loadDDElements();"
}
if {[info exists savecachechange]} {
  wpSaveMenuJavascript browse $c $f $defc morcInBrowseDone $savecachechange
}
wpStatusAndNewmailJavascript
cgi_puts "if(self.loadDDElements) loadDDElements();"
cgi_puts "</script>"

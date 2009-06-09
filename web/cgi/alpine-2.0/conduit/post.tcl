#!./tclsh
# $Id: post.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  post.tcl
#
#  Purpose:  CGI script to do the job of posting message supplied by compose form
# 

#  Input: 
set post_vars {
  {cid		"Missing Command ID"}
  {check	{}	0}
  {sendop	{}	""}
  {extrahdrs	{}	""}
  {fccattach	{}	0}
  {form_flowed	{}	""}
  {subtype	{}	"plain"}
  {priority	{}	""}
  {autodraftuid {}	0}
}

# inherit global config
source ../alpine.tcl
source ../common.tcl

#  Output: HTML containing javascript calls to functions of parent window
# 


proc fieldname {name} {
  regsub -all -- {-} [string tolower $name] {_} fieldname
  return $fieldname
}

proc expand_address_field {field _msgdata} {
  global has_fcc

  upvar 1 $_msgdata msgdata

  set fn [fieldname $field]
  for {set i 0} {$i < [llength $msgdata]} {incr i} {
    if {[string length [lindex [lindex $msgdata $i] 1]]} {
      set fld [lindex $msgdata $i]
      if {[string compare [fieldname [lindex $fld 0]] $fn] == 0} {
	if {[catch {WPCmd PEAddress expand [lindex $fld 1] fcc} expaddr]} {
	  WPCmd PEInfo statmsg "Can't expand $field: $expaddr"
	} else {
	  if {[lindex $expaddr 1] != 0} {
	    if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
	      # addresses and ldapaddrs should be set at this point
	      upvar 1 addresses a
	      upvar 1 ldapquery l
	      upvar 1 field f
	      set a [lindex $expaddr 0]
	      set l [lindex $expaddr 1]
	      set f $fn
	      return 1
	    } else {
	      # else fall thru back into composer
	      WPCmd PEInfo statmsg "Compose Error: $errstr"
	      break
	    }
	  } elseif {[string compare [lindex $expaddr 0] [lindex $fld 1]]} {
	    set msgdata [lreplace $msgdata $i $i [list [lindex $fld 0] [lindex $expaddr 0]]]

	    # set fcc?
	    set fccfn [lindex $expaddr 2]
	    set fccdef [WPCmd PECompose fccdefault]
	    if {[string compare to [string tolower $fn]] == 0 && [string length $fccfn]
		&& (![info exists has_fcc] || 0 == [string compare [lindex $fccdef 1] [lindex $has_fcc 1]])} {
	      for {set j 0} {$j < [llength $msgdata]} {incr j} {
		if {[string compare fcc [fieldname [lindex [lindex $msgdata $j] 0]]] == 0} {
		  set fcc_index $j
		  break
		}
	      }

	      set colid [lindex $fccdef 0]
	      if {[info exists fcc_index]} {
		if {[string compare $fccfn [lindex [lindex [lindex $msgdata $fcc_index] 1] 1]]} {
		  lappend msgdata [list postoption [list fcc-set-by-addrbook 1]]
		}

		set msgdata [lreplace $msgdata $fcc_index $fcc_index [list Fcc [list $colid $fccfn]]]
	      } else {
		lappend msgdata [list Fcc [list $colid $fccfn]]
		lappend msgdata [list postoption [list fcc-set-by-addrbook 1]]
	      }

	      set has_fcc [list $colid $fccfn]
	    }
	  }
	}
      }
    }
  }

  return 0
}

proc removeAutoDraftMsg {uid} {
  if {$uid != 0} {
    if {[regexp {^[0-9]+$} $uid]} {
      if {[catch {WPCmd PEPostpone delete $uid} result]} {
	catch {WPCmd PEInfo statmsg "Stale autodraft UID: $result"}
      }
    } else {
      catch {WPCmd PEInfo statmsg "Invalid autodraft uid: $uid"}
    }
  }
}

# Import data validate it and get session id
if {[catch {WPGetInputAndID sessid}]} {
  return
}

# grok parameters
foreach item $post_vars {
  if {[catch {eval WPImport $item} errstr]} {
    lappend errs $errstr
  }
}


# collect message data

# For now the input headers have to match the postheaders
# list.  Any outside the list are ignored (and probably should
# be to avoid hostile input).  Note, postheaders is a
# super-set of composeheaders as not all headers are meant
# to be shown the user for composition
if {[catch {WPCmd PECompose userhdrs} headers]} {
  error [list _action "User Headers" $headers "Click browser's Back button to try again."]
}

if {[catch {WPCmd PECompose syshdrs} otherhdrs]} {
  error [list _action "System Headers" $otherhdrs "Click browser's Back button to try again."]
} else {
  eval "lappend headers $otherhdrs"
}

foreach field $headers {
  set hdr [string tolower [lindex $field 0]]
  regsub -all -- {-} $hdr {_} hdr
  WPLoadCGIVarAs $hdr val
  switch -- $hdr {
    attach {
      # disregard: u/i convenience (attachments marshalled below)
    }
    fcc {
      if {[string length $val]} {
	WPLoadCGIVar colid
	set has_fcc [list $colid $val]
	lappend msgdata [list Fcc $has_fcc]
      }
    }
    default {
      if {[string length $val] || [lsearch -exact {subject} $hdr] >= 0} {
	# join lines
	regsub {\n} $val { } val
	# strip trailing whitespace
	set val [string trim $val]
	# any any trailing comma
	regsub {,$} $val {} val

	set hdrvals($hdr) $val
	lappend msgdata [list [lindex $field 0] $val]
	if {[lsearch -exact {to cc bcc} $hdr] >= 0} {
	  set has_$hdr 1
	}
      }
    }
  }
}

if {[info exists env(REMOTE_ADDR)]} {
  lappend msgdata [list x-auth-received "from \[$env(REMOTE_ADDR)\] by [info hostname] via HTTP; [clock format [clock seconds] -format "%a, %d %b %Y %H:%M:%S %Z"]"]
}

if {[catch {cgi_import attachments}] == 0} {
  foreach id [split $attachments ","] {
    lappend msgdata [list attach $id]
  }
}

WPLoadCGIVar body

# pass body text options, dress as necessary
if {0 == [string compare -nocase $subtype html]} {
  lappend msgdata [list postoption [list subtype html]]
  set body "<html><head><title></title></head>\n<body>\n${body}\n</body></html>"
  set compose_mode rich
} else {
  set compose_mode plain
  if {[string length $form_flowed]} {
    lappend msgdata [list postoption [list flowed yes]]
  }
}

if {[regexp {^(lowest|low|normal|high|highest)$} $priority]} {
  lappend msgdata [list postoption [list priority $priority]]
}

lappend msgdata [list body [split $body "\n"]]

switch -exact -- $fccattach {
  0 -
  1 {
    lappend msgdata [list postoption [list fcc-without-attachments [expr {!$fccattach}]]]
  }
}

lappend msgdata [list postoption [list charset "UTF-8"]]

# figure out what to do with data
if {[string compare $sendop send] == 0} {
  if {[info exists has_to] || [info exists has_cc] || [info exists has_bcc] || [info exists has_fcc]} {
    # expand any nicknames
    if {[catch {
      set fccdef [WPCmd PECompose fccdefault]
      for {set i 0} {$i < [llength $msgdata]} {incr i} {
	if {[string length [lindex [lindex $msgdata $i] 1]]} {
	  set fld [lindex $msgdata $i]
	  set fn [string tolower [lindex $fld 0]]
	  switch -- $fn {
	    [Ff]cc {
	      if {[string length [lindex [lindex $fld 1] 1]]} {
		# setup for send confirmation
		set colidval [lindex [lindex $fld 1] 0]
		set fccval [lindex [lindex $fld 1] 1]
	      }
	    }
	    to -
	    cc -
	    bcc -
	    reply-to {
	      set expaddr [WPCmd PEAddress expand [lindex $fld 1] {}]
	      if {[string compare [lindex $expaddr 0] [lindex $fld 1]]} {
		set msgdata [lreplace $msgdata $i $i [list [lindex $fld 0] [lindex $expaddr 0]]]

		# if expanded, update fcc?
		if {[string compare to $fn] == 0 && [string length $fn]} {
		  set expanded_fcc [lindex $expaddr 2]
		}
	      }
	    }
	    body {
	      if {[string length $form_flowed]} {
		set ws "\[ \t]"
		set nws "\[^ \t]"

		set nextline [lindex [lindex $fld 1] 0]
		for {set j 1} {$j <= [llength [lindex $fld 1]]} {incr j} {
		  set line $nextline
		  # space stuff?
		  if {[regexp "^${ws}+" $line]} {
		    set line " $line"
		  }

		  set nextline [lindex [lindex $fld 1] $j]
		  if {[regexp {^-- $} $line] == 0} {
		    catch {unset linetext}
		    # trim trailing WS from lines preceding those with LWS (space-stuff as needed)
		    if {[string length $nextline] == 0 || [regexp "^${ws}+(${nws}?.*)\$" $nextline dummy linetext]} {
		      set line [string trimright $line]
		      if {[info exists linetext] == 0 || [string length $linetext] == 0} {
			set nextline ""
		      }
		    }

		    # break overly long lines in a flowed way
		    if {[regexp {^[^>]} $line] && [string length $line] > 1000} {
		      while {[regexp "^(${ws}*${nws}+${ws}+)$nws" [string range $line 900 end] dummy linex]} {
			set cliplen [expr {900 + [string length $linex]}]
			lappend newbody [string range $line 0 [expr {$cliplen - 1}]]
			set line [string range $line $cliplen end]
		      }
		    }
		  }

		  lappend newbody $line
		}

		set msgdata [lreplace $msgdata $i $i [list body $newbody]]
	      }
	    }
	    default {
	    }
	  }
	}
      }
    } result]} {
      WPCmd PEInfo statmsg "Address problem: $result"
    } else {
      # update fcc?
      if {[info exists expanded_fcc]
	  && (![info exists has_fcc] || 0 == [string compare [lindex $fccdef 1] [lindex $has_fcc 1]])} {
	for {set j 0} {$j < [llength $msgdata]} {incr j} {
	  if {[string compare fcc [fieldname [lindex [lindex $msgdata $j] 0]]] == 0} {
	    set fcc_index $j
	    break
	  }
	}

	set colid [lindex $fccdef 0]
	if {[info exists fcc_index]} {
	  set msgdata [lreplace $msgdata $fcc_index $fcc_index [list Fcc [list $colid $expanded_fcc]]]
	} else {
	  lappend msgdata [list Fcc [list $colid $expanded_fcc]]
	}
      }

      removeAutoDraftMsg $autodraftuid

      # do the sending...
      set verb Send
      set verbpast Sent
      set postcmd PECompose
      set postcmdopt post
      if {[info exists compose_mode]} {
	catch {WPSessionState compose_mode $compose_mode}
      }
    }
  } else {
    set reportfunc sendFailure
    set postresult "Send MUST include Recipients (To, Cc, Bcc, or Fcc)"
  }
} elseif {[string compare $sendop postpone] == 0} {
  removeAutoDraftMsg $autodraftuid
  set verb "Save to Drafts"
  set verbpast "Saved to Drafts"
  set postcmd PEPostpone
  set postcmdopt append
} elseif {[string compare $sendop autodraft] == 0} {
  removeAutoDraftMsg $autodraftuid

  set verb "Save to Drafts"
  set verbpast "Saved to Drafts"
  set postcmd PEPostpone
  set postcmdopt append
  set reportfunc reportAutoDraft
} elseif {[string length $extrahdrs] > 0} {
  # save msgdata to servlet
  set errstr "what is the deal with extra headers"
  if {0 && [catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
BUG: deal with this?
    if {[catch {WPCmd PEInfo set wp_extra_hdrs} extras] || $extras == 1} {
      set toggle 0
    } else {
      set toggle 1
    }

    catch {WPCmd PEInfo set wp_extra_hdrs $toggle}

    _cgi_set_uservar restore 1
    set src compose
  } else {
    # else fall thru back into composer
    set reportfunc sendFailure
    set postresult "Compose Error: $errstr"
  }
} else {
  set reportfunc sendFailure
  set postresult "Unrecognized Action"
}

#do what was asked
if {[info exists postcmd]} {
  if {[info exists msgdata]} {
    if {[catch {WPCmd $postcmd $postcmdopt $msgdata} postresult]} {
      set auth [wpHandleAuthException $postresult [list 0 "send"]]
      if {[string length $auth]} {
	set reportcall "processPostAuthException(\{${auth}\});"
      } else {
	set reportfunc sendFailure
      }
    } elseif {0 == [string compare $postcmd PECompose]} {
      WPCmd PEInfo statmsg "Message $verbpast"
    }
  } else {
    WPCmd PEInfo statmsg "No Message $verbpast!"
  }

  if {[info exists delete_me]} {
    foreach i $delete_me {
      catch {file delete $i}
    }
  }
} elseif {![info exists src]} {
  set style ""
  set title "Compose Message"
  catch {unset attachments}

  # regurgitate the compose window
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    _cgi_set_uservar restore 1
    set src compose

    unset body
  }
}

puts stdout "Content-type: text/html;\n\n<html><head><script>"

if {[info exists reportfunc]} {
  puts stdout "window.parent.${reportfunc}(\"${postresult}\");"
} elseif {[info exists reportcall]} {
  puts stdout "window.parent.${reportcall};"
} else {
  puts stdout "window.parent.sendSuccess(\"${postresult}\");"
}

puts stdout "</script></head><body></body></html>"

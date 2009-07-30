# $Id: post.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  post.tcl
#
#  Purpose:  CGI script to perform message posting via compose.tcl
#	     generated form

#  Input: 
set post_vars {
  {cid		"Missing Command ID"}
  {action	{}	""}
  {send		{}	0}
  {postpone	{}	0}
  {cancel	{}	0}
  {check	{}	0}
  {br_to	{}	0}
  {br_cc	{}	0}
  {br_bcc	{}	0}
  {br_reply_to	{}	0}
  {br_fcc	{}	0}
  {ex_to	{}	""}
  {ex_cc	{}	""}
  {ex_bcc	{}	""}
  {ex_reply_to	{}	""}
  {sendop	{}	""}
  {queryattach	{}	0}
  {attach	{}	0}
  {detach	{}	0}
  {extrahdrs	{}	""}
  {help		{}	""}
  {postpost	{}	"main.tcl"}
  {fccattach	{}	0}
  {form_charset	{}	""}
  {form_flowed	{}	""}
}

# NOT Input
catch {
  unset src
}

#  Output: 
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

proc chartest_value {entity} {
  global _cgi

  if {[catch {cgi_import_as ke_${entity} tc}] == 0} {
    set tcval ""
    if {[set j [string length $tc]]} {
      for {set i 0} {$i < $j} {incr i} {
	binary scan [string index $tc $i] c x
	set x [expr ($x & 0xff)]
	lappend tcval [format {%o} $x]
      }
    }

    return $tcval
  } else {
    error "Unset testchar_$entity"
  }
}

## read vars
foreach item $post_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {$cid != [WPCmd PEInfo key]} {
  error [list _action Postpone "Invalid Operation ID" "Click Back button to try again."]
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
lappend msgdata [list body [split $body "\n"]]


switch -exact -- $fccattach {
  0 -
  1 {
    lappend msgdata [list postoption [list fcc-without-attachments [expr {!$fccattach}]]]
  }
}

# pass on form's charset?
# TURNED OFF since all compose form interaction BETTER be UTF-8
if {0 && [string length $form_charset]} {
  # messy charset heuristics
  # idea is to look for planted HTML entities and see if known
  # encoding transliterations have occured. inspired by:
  # <http://www.cs.tut.fi/~jkorpela/chars.html#encinfo>

  # test for:
  #  entity        values
  # euro (#8364)
  # cyrillic shcha (#1060)
  # iso-8859-15 (Latin0): euro IS 200
  # iso-8859-1 (Latin1): thorn IS 376 or U+C3BE BUT NOT &#8220; &#254; OR &thorn;
  # Unicode literal full width yen: U+FFE5 IS 215F (ISO-2022-JP), A1EF (EUC-JP), or 818F (Shift-JIS) and so on

  # remember, the first element of each group MUST appear in compose.tcl, too
  set cstests {}
  set xcstests {
    {#8364	{{{40 254} ISO-10646} {{342 202 254} UTF-8} {244 ISO-8859-15} {325 IBM-850}} {}}
    {#1066	{{{377} KOI8-R} {312 ISO-8859-5}} {}}
    {thorn	{{376 ISO-8859-1}} {{303 276} UTF-8} {iso-8859-1 {{46 43 70 62 62 60 73} {46 43 62 65 64 73} {46 164 150 157 162 156 73}}}}
    {tcedil	{{376 ISO-8859-2}} {{46 164 143 145 144 151 154 73}}}
    {#65509	{{{302 245} UTF-8} {{241 315} EUC-KR} {{243 244} GB2312} {{242 104} BIG5} {{241 357} EUC-JP} {{201 217} Shift-JIS} {{33 44 102 41 157 33 50 102} ISO-2022-JP}} {}}
  }

  catch {unset test_charset}
  foreach cs $cstests {
    # asked for test entity available?
    if {[catch {chartest_value [lindex $cs 0]} ctest] == 0} {
      # test for positive [re]encoding assertions
      foreach testpos [lindex $cs 1] {
	if {[regexp "^[lindex $testpos 0]\$" $ctest]} {
	  set test_charset [lindex $testpos 1]
	  break
	}
      }

      if {![info exists test_charset]} {
	set csneg [lindex [lindex $cs 2] 0]
	foreach testneg [lindex [lindex $cs 2] 1] {
	  if {[regexp "^$testneg\$" $ctest]} {
	    if {[info exists form_charset]
		&& [string compare [string tolower $form_charset] $csneg] == 0} {
	      unset form_charset
	      break
	    }
	  }
	}
      } else {
	break
      }
    }
  }

  if {[info exists test_charset]} {
    lappend msgdata [list postoption [list charset $test_charset]]
  } elseif {[info exists form_charset]} {
    lappend msgdata [list postoption [list charset $form_charset]]
  } else {
    lappend msgdata [list postoption [list charset "X-UNKNOWN"]]
  }
} else {
  lappend msgdata [list postoption [list charset "UTF-8"]]
}

# pass on text fomat=flowed?
if {[string length $form_flowed]} {
  lappend msgdata [list postoption [list flowed yes]]
}

# figure out what to do with data
if {[string compare OK [string trim $action]] == 0 && ($send || [string compare $sendop send] == 0)} {
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

      # do the sending...
      set verb Send
      set verbpast Sent
      set postcmd PECompose
      set postcmdopt post
    }
  } else {
    WPCmd PEInfo statmsg "Send MUST include Recipients (To, Cc, Bcc, or Fcc)"
  }
} elseif {[string compare OK [string trim $action]] == 0 && ($postpone || [string compare $sendop postpone] == 0)} {
  set verb Postpone
  set verbpast Postponed
  set postcmd PEPostpone
  set postcmdopt append
} elseif {$help == 1 || [string compare "get help" [string tolower $help]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    _cgi_set_uservar oncancel "compose&restore=1"
    set src help
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$check == 1 || [string compare spell [string tolower [string range $check 0 4]]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    set src spell
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$queryattach == 1 || [string compare "add attachment" [string tolower $queryattach]] == 0 || [string compare "attach" [string tolower $queryattach]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    set src askattach
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$br_to == 1 || [string compare browse [string tolower $br_to]] == 0 || [string compare to [string tolower $br_to]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    set oncancel compose
    _cgi_set_uservar op browse
    _cgi_set_uservar field to
    set src addrbrowse
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$br_cc == 1 || [string compare browse [string tolower $br_cc]] == 0 || [string compare cc [string tolower $br_cc]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    set oncancel compose
    _cgi_set_uservar op browse
    _cgi_set_uservar field cc
    set src addrbrowse
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$br_bcc == 1 || [string compare browse [string tolower $br_bcc]] == 0 || [string compare bcc [string tolower $br_bcc]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    set oncancel compose
    _cgi_set_uservar op browse
    _cgi_set_uservar field bcc
    set src addrbrowse
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$br_reply_to == 1 || [string compare browse [string tolower $br_reply_to]] == 0 || [string compare "reply_to" [string tolower $br_reply_to]] == 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    set oncancel compose
    _cgi_set_uservar op browse
    _cgi_set_uservar field reply-to
    set src addrbrowse
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {$br_fcc == 1 || ($br_fcc > 0 && [string length $br_fcc] > 0)} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
    # fake cgi input for script
    _cgi_set_uservar onselect compose
    _cgi_set_uservar oncancel compose
    set src fldrbrowse
  } else {
    # else fall thru back into composer
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {[string compare expand [string tolower $ex_to]] == 0} {
  if {[expand_address_field To msgdata]} {
    set src ldapbrowse
  }
} elseif {[string compare expand [string tolower $ex_cc]] == 0} {
  if {[expand_address_field Cc msgdata]} {
    set src ldapbrowse
  }
} elseif {[string compare expand [string tolower $ex_bcc]] == 0} {
  if {[expand_address_field Bcc msgdata]} {
    set src ldapbrowse
  }
} elseif {[string compare expand [string tolower $ex_reply_to]] == 0} {
  if {[expand_address_field Reply-To msgdata]} {
    set src ldapbrowse
  }
} elseif {[string length $extrahdrs] > 0} {
  # save msgdata to servlet
  if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
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
    WPCmd PEInfo statmsg "Compose Error: $errstr"
  }
} elseif {[string compare OK [string trim $action]] == 0 && ($cancel || [string compare $sendop cancel] == 0)} {
  # clean up attachments
  WPCmd PEInfo statmsg "Message cancelled"
  catch {WPCmd PEInfo unset suspended_composition}
  catch {WPCmd PEInfo unset wp_extra_hdrs}
  set src ""
} else {
  # check for per-attachment ops
  if {[info exists attachments]} {
    set a [split $attachments ","]
    for {set i 0} {$i < [llength $a]} {incr i} {
      if {[catch {cgi_import detach_[lindex $a $i].x}] == 0} {
	if {[catch {WPCmd PECompose unattach [lindex $a $i]} result]} {
	  WPCmd PEInfo statmsg "Unattach: $result"
	} else {
	  set attachment_deleted [lindex $a $i]

	  set a [lreplace $a $i $i]
	  set attachments [join $a ","]

	  for {set i 0} {$i < [llength $msgdata]} {incr i} {
	    if {[string compare attach [lindex [lindex $msgdata $i] 0]] == 0 && [lindex [lindex $msgdata $i] 1] == $attachment_deleted} {
	      set msgdata [lreplace $msgdata $i $i]
	      break
	    }
	  }

	  WPCmd PEInfo statmsg "Attachment Removed"
	}

	break
      }
    }
  }

  if {![info exists attachment_deleted]} {
    WPCmd PEInfo statmsg "Unrecognized Action"
  }
}

#do what was asked
if {[info exists postcmd]} {
  if {[info exists msgdata]} {
    if {[catch {WPCmd $postcmd $postcmdopt $msgdata} errstr]} {
      # if auth problem, save msgdata for after we ask for credentials
      if {([string compare NOPASSWD [string range $errstr 0 7]] == 0 || [string compare BADPASSWD [string range $errstr 0 8]] == 0)
          && [catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {

	if {[catch {WPCmd PEInfo authrequestor} server]} {
	  append reason "Unknown server asking for authentication.  Press cancel to abort if you think this message is in error."
	} else {
	  append reason "[cgi_nl]Enter Username and Password to connect to [cgi_bold $server]"
	  lappend params [list server $server]
	}

	if {[catch {WPCmd PESession creds 0 "{$server}"} creds] == 0 && $creds != 0} {
	  catch {WPCmd PEInfo statmsg "Invalid Username or Password"}
	  WPCmd PESession nocred 0 "{$server}"
	}

	WPCmd set reason "The server ($server) used to send this message requires authentication.[cgi_nl]"

	WPCmd set cid [WPCmd PEInfo key]
	WPCmd set authcol 0
	WPCmd set authfolder "{$server}"
	WPCmd set authpage [WPPercentQuote "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=dosend"]
	WPCmd set authcancel [WPPercentQuote "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=compose&restore=1&cid=$cid"]

	set src auth

      } else {
	# regurgitate the compose window
	set style ""
	set title "$verb Error: [cgi_font class=notice "$errstr"]"
	if {[string length $errstr]} {
	  set notice "$verb FAILED: $errstr"
	} else {
	  set notice "$verb FAILED: [WPCmd PEInfo statmsg]"
	}

	WPCmd PEInfo statmsg "$notice"

	# regurgitate the compose window
	if {[catch {WPCmd PEInfo set suspended_composition $msgdata} errstr] == 0} {
	  _cgi_set_uservar restore 1
	  set src compose

	  unset body
	} else {
	}

	set src compose
      }
    } else {
      catch {WPCmd PEInfo unset suspended_composition}
      WPCmd PEInfo statmsg "Message $verbpast!"
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

if {[info exists src] && [string length $src]} {
  source [WPTFScript $src]
} else {
  cgi_redirect "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=$postpost"
}

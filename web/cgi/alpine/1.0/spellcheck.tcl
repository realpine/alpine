#!./tclsh
# $Id: spellcheck.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  spellcheck.tcl
#
#  Purpose:  CGI script to generate html form used to check
#            body text spelling in the webpine-lite composer

#  Input:
set query_vars {
  {repqstr	""	""}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the page to correct spelling

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl


set check_menu {
  {
    {expr 0}
    {
      {
	# * * * * DONE * * * *
	cgi_puts "<fieldset>"

	cgi_puts [cgi_font size=-1 "When finished, choose action below, then click [cgi_italic OK].[cgi_nl][cgi_nl]"]
	cgi_radio_button spell=spell
	cgi_puts "[cgi_nbspace][cgi_font color=white face=Helvetica size=-1 Correct][cgi_nl]"
	cgi_radio_button spell=cancel
	cgi_puts "[cgi_nbspace][cgi_font color=white face=Helvetica size=-1 Cancel][cgi_nl]"
	cgi_br
	#cgi_image_button action=[WPimg but_s_do] border=0 alt="Do"
	cgi_division "style=padding-bottom:4" align=center {
	  cgi_submit_button "action=OK" class=navtext
	}
	cgi_puts "</fieldset>"
      }
    }
  }
  {
    {}
    {
      {
	# * * * * Done * * * *
	cgi_submit_button "spell=Apply" class=navtext
      }
    }
  }
  {
    {}
    {
      {
	# * * * * Cancel * * * *
	cgi_submit_button "spell=Cancel" class=navtext
      }
    }
  }
  {
    {expr 0}
    {
      {
	# * * * * Help * * * *
	cgi_submit_button "help=Get Help" class=navtext
      }
    }
  }
}

set done 0
set first 1
set spellresult {}
set line {}

proc spelled {pipe} {
  global done spellresult line first

  if {[eof $pipe]} {
    catch {close $pipe}
    set done 1
    return
  }

  gets $pipe w

  if {$first == 0} {
    if {[string length $w]} {
      lappend line $w
    } else {
      lappend spellresult $line
      set line {}
    }
  } else {
    set first 0
  }
}


WPEval $query_vars {
  if {[catch {WPCmd PEInfo set suspended_composition} msgdata]} {
    set problem "Can't read message text"
  } else {
    foreach p $msgdata {
      if {[string compare [lindex $p 0] body] == 0} {
	set body [lindex $p 1]
	break
      }
    }

    if {![info exists body]} {
      set problem "Can't find body in message text"
    } else {
      # spell check and gather results
      # set tmpfile 
      for {set i 0} {$i < 5} {incr i} {
	set tmpfile [file join $_wp(sockdir) "sc[pid][expr rand()]"]
	if {[file exists $tmpfile] == 0} {
	  if {[catch {open $tmpfile w} ofp] == 0} {
	    break
	  }
	}
	unset tmpfile
      }

      if {![info exists tmpfile]} {
	set problem "Can't create temporary file"
      }
    }
  }

  if {![info exists problem]} {
    if {[string length $repqstr]} {
      set quoter $repqstr
    } else {
      set quoter "> "
    }

    foreach l $body {
      if {[string compare $l "---------- Forwarded message ----------"] == 0} {
	break;
      } elseif {[regexp "^$quoter" $l]} {
	puts $ofp ""
      } else {
#	regsub -all {\$} $l {\$} l
	puts $ofp "^${l}"
      }
    }

    close $ofp

    set cmd [list $_wp(ispell) "-a"]
    set pipe [open "|$cmd < $tmpfile 2> /dev/null" r]

    fileevent $pipe readable [list spelled $pipe]

    vwait done

    catch {file delete $tmpfile}
  }

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Check Spelling"
      WPStyleSheets
      cgi_put  "<style type='text/css'>"
      cgi_put  ".correction	{ font-family: geneva, arial, sans-serif ; font-size: 9pt }"
      cgi_puts "</style>"
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" {
      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post enctype=multipart/form-data target=_top {
	cgi_text "page=compose" type=hidden notab
	cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
	cgi_text "restore=1" type=hidden notab
	cgi_text "style=Spell" type=hidden notab
	cgi_text "last_line=[llength $spellresult]" type=hidden notab
	if {[string length $repqstr]} {
	  cgi_text "repqstr=$repqstr" type=hidden notab
	}

	cgi_table border=0 cellspacing=0 cellpadding=2 width="100%" height="100%" {
	  cgi_table_row {
	    eval {
	      cgi_table_data $_wp(menuargs) {
		WPTFCommandMenu check_menu {}
	      }
	    }

	    cgi_table_data valign=top class=dialog {

	      set badlines {}
	      array set badwords {}
	      set badcount 0
	      for {set n 0} {$n < [llength $spellresult]} {incr n} {
		set words {}
		foreach ms [lindex $spellresult $n] {
		  if {[regexp {& ([a-zA-Z0-9]*) [0-9]+ ([0-9]+):[ ]?(.*)$} $ms match w o s]} {
		    incr badcount
		    if {[regsub -all {, } $s { } sugs] < 0} {
		      continue
		    }

		    lappend words [list $w [expr {$o - 1}] $sugs]
		    if {[info exists badwords($w)]} {
		      incr badwords($w)
		    } else {
		      set badwords($w) 1
		    }
		  } elseif {[regexp {# ([a-zA-Z0-9]*) ([0-9]+)$} $ms match w o]} {
		    incr badcount
		    lappend words [list $w [expr {$o - 1}] {}]
		    if {[info exists badwords($w)]} {
		      incr badwords($w)
		    } else {
		      set badwords($w) 1
		    }
		  }
		}

		if {[llength $words]} {
		  lappend badlines [list $n $words]
		}
	      }

	      if {[info exists problem] || $badcount <= 0} {
		cgi_table align=center valign=top height="100%" {
		  cgi_table_row {
		    cgi_table_data align=center valign=bottom heigh="20%" {
		      if {[info exists problem]} {
			cgi_puts "Problem detected: $problem"
		      } else {
			cgi_puts "No misspelled words found."
		      }
		    }
		  }
		  cgi_table_row {
		    cgi_table_data align=center valign=top {
		      cgi_put "Click "
		      cgi_submit_button "cancel=Continue" class=navtext
		      cgi_put " to return to your composition."
		    }
		  }
		}
	      } else {
		cgi_table width="95%" border=0 align=center valign=top {
		  cgi_table_row {
		    cgi_table_data align=center "style=padding-top:10;padding-bottom:10" {
		      cgi_puts "Web Alpine found [cgi_bold $badcount] possibly misspelled word[WPplural $badcount]."
		      cgi_puts "Grouped by the line on which they were found, misspelled words can be corrected by either selecting from the list of suggestions, when available (note, first option always blank), or entering the corrected spelling directly."
		      cgi_puts "When finished click [cgi_italic Apply] to correct the text, or [cgi_italic Cancel] to return to the composition unchanged."
		    }
		  }

		  foreach sl $badlines {
		    set lnum [lindex $sl 0]
		    set locs {}

		    cgi_table_row {
		      cgi_table_data bgcolor=white align=left height=20 class=view "style=font-family:courier;padding:8" {
			set ol [lindex $body $lnum]
			set l ""
			set o 0
			foreach w [lindex $sl 1] {
			  set offset [lindex $w 1]
			  set word [lindex $w 0]
			  set wordlen [string length $word]
			  append l "[cgi_quote_html [string range $ol $o [expr {$offset - 1}]]][cgi_url $word "#${lnum}_[lindex $w 1]_${wordlen}" class=mispell]"
			  set o [expr {$offset + $wordlen}]
			}

			append l [string range $ol $o end]

			cgi_put $l
		      }
		    }

		    cgi_table_row {
		      cgi_table_data {
			cgi_table width=90% align=center border=0 {

			  foreach w [lindex $sl 1] {
			    set word [lindex $w 0]
			    set wordlen [string length $word]
			    set wordloc "${lnum}_[lindex $w 1]_${wordlen}"
			    
			    cgi_table_row {

				if {[llength [lindex $w 2]] > 0} {
				  cgi_table_data align=left class=correction nowrap {
				    cgi_put "Replace [cgi_anchor_name $wordloc][cgi_bold $word] with "
				  }

				  cgi_table_data align=left class=correction nowrap {
				    cgi_select s_${wordloc} class=correction {
				      cgi_option "" value=
				      foreach sug [lindex $w 2] {
					cgi_option $sug value=$sug
				      }
				    }
				  }

				  cgi_table_data align=left class=correction nowrap {
				    cgi_put " or change to "
				  }
				} else {
				  cgi_table_data align=left class=correction nowrap {
				    cgi_put "Change [cgi_anchor_name $wordloc][cgi_bold $word] to "
				  }
				}

				cgi_table_data align=left class=correction nowrap {
				  cgi_text r_${wordloc}= "size=[expr {$wordlen + 4}]" maxlength=64 class=correction
				}

				cgi_table_data align=left class=correction width=90% colspan=3 {

				  if {$badwords($word) > 1} {
				    if {![info exists badseen($word)]} {
				      switch $badwords($word) {
					2 { set badtimes both }
					default { set badtimes "all $badwords($word)" }
				      }

				      cgi_put " and "
				      cgi_checkbox a_${wordloc} 
				      cgi_put " apply to $badtimes occurrences"
				      set badseencount($word) 1
				    } else {
				      incr badseencount($word)
				      switch $badseencount($word) {
					2 { set bad1 "second " ; set bad2 "" }
					3 { set bad1 "third " ; set bad2 "" }
					4 { set bad1 "fourth " ; set bad2 "" }
					5 { set bad1 "fifth " ; set bad2 "" }
					6 { set bad1 "sixth " ; set bad2 "" }
					default { set bad1 "" ; set bad2 " $badseencount($word)" }
				      }
				      cgi_put "(${bad1}occurrence${bad2})"
				    }

				    lappend badseen($word) $wordloc
				  } else {
				    cgi_put [cgi_img [WPimg dot2]]
				  }
			      }

			      lappend locs $wordloc
			    }
			  }

			  cgi_table_row {
			    cgi_table_data height=8 {
			      cgi_text "l_$lnum=[join $locs {,}]" type=hidden notab
			    }
			  }
			}
		      }
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center height=50 {
		      foreach l [array names badseen] {
			set m $badseen($l)
			cgi_text "e_[lindex $m 0]=[join [lrange $m 1 end] {,}]" type=hidden notab
		      }

		      cgi_submit_button "spell=Apply" class=navtext
		      cgi_submit_button "spell=Cancel" class=navtext
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
}


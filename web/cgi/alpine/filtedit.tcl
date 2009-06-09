#!./tclsh
# $Id: filtedit.tcl 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  filtedit.tcl
#
#  Purpose:  CGI script to generate html form editing of a single filter
#
#  Input: 
set filtedit_vars {
  {cid     "No cid"}
  {oncancel "No oncancel"}
  {onfiltcancel {} ""}
  {fno     {}  -1}
  {add     {}  0}
  {filterrtext {}  ""}
}

#  Output:
#

## read vars
foreach item $filtedit_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      if {[llength $item] > 2} {
	set [lindex $item 0] [lindex $item 2]
      } else {
	error [list _action [lindex $item 1] $result]
      }
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {[info exists filtedit_add]} {
  set add $filtedit_add
}
if {[info exists filtedit_fno]} {
  set fno $filtedit_fno
}
if {[info exists filtedit_onfiltcancel]} {
  set onfiltcancel $filtedit_onfiltcancel
}
if {[info exists filtedit_filterrtext]} {
  set filterrtext $filtedit_filterrtext
}

set filterr 0
if {[string length $filterrtext]} {
  set filterr 1
}

set filtedit_menu {
  {
    {}
    {
      {
	# * * * * OK * * * *
	#cgi_image_button filt_save=[WPimg but_save] border=0 alt="Save Config"
	cgi_submit_button "filt_save=Save Filter"
      }
    }
  }
  {
    {}
    {
      {
	# * * * * CANCEL * * * *
	cgi_submit_button filtcancel=Cancel
      }
    }
  }
  {
    {}
    {
      {
	# * * * * HELP * * * *
	cgi_submit_button "filthelp=Get Help"
      }
    }
  }
}


proc wpGetVarAs {_var _varas} {
  upvar $_varas varas

  if {[catch {cgi_import_as $_var varas} result]} {
    set varas ""
  }
}

proc freetext_cell {intro varname varval} {
  cgi_table_data align=right {
    cgi_puts [cgi_bold "$intro :[cgi_nbspace][cgi_nbspace]"]
  }
  cgi_table_data align=left {
    cgi_text "$varname=$varval" "style=margin:2"
  }
}


set patvarnamesl {
    to {"To" "" freetext}
    from {"From" "" freetext}
    sender {"Sender" "" freetext}
    cc {"Cc" "" freetext}
    recip {"Recipients" "" freetext}
    partic {"Participants" "" freetext}
    news {"Newsgroups" "" freetext}
    subj {"Subject" "" freetext}
    alltext {"All Text" "" freetext}
    bodytext {"Body Text" "" freetext}
    age {"Age Interval" "" freetext}
    size {"Size Interval" "" freetext}
    score {"Score Interval" "" freetext}
    keyword {"Keyword" "" freetext}
    charset {"Character Set" "" freetext}
    headers {"Extra Headers" "" headers}
    stat_new {"Message is New" "either" status}
    stat_rec {"Message is Recent" "either" status}
    stat_del {"Message is Deleted" "either" status}
    stat_imp {"Message is Important" "either" status}
    stat_ans {"Message is Answered" "either" status}
    stat_8bitsubj {"Subject contains raw 8bit characters" "either" status}
    stat_bom {"Beginning of Month" "either" status}
    stat_boy {"Beginning of Year" "either" status}
    addrbook {"Address in address book" "" addrbook}
}

array set patvarnames $patvarnamesl


set actionvarnamesl {kill folder move_only_if_not_deleted}
array set actionvarnames {
    kill {"status" 0}
    kill {"kill" 0}
    folder {"Folder" ""}
    move_only_if_not_deleted {"moind" 0}
}

cgi_http_head {
  cgi_content_type
  cgi_pragma no-cache
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "Filter List Configuration"
    WPStyleSheets
  }

  if {$add == 0} {
    set fext [WPCmd PEConfig filtextended $fno]

    foreach fvar $fext {
      switch -- [lindex $fvar 0] {
	pattern {
	  set patternvars [lindex $fvar 1]
	  foreach patternvar $patternvars {
	    set patname [lindex $patternvar 0]
	    if {[info exists patvarnames($patname)]} {
	      set oldval $patvarnames($patname)
	      set patvarnames($patname) [list [lindex $oldval 0] [lindex $patternvar 1]]
	    }
	  }
	}
	filtaction {
	  set actionvars [lindex $fvar 1]
	  foreach actionvar $actionvars {
	    set actionname [lindex $actionvar 0]
	    if {[info exists actionvarnames($actionname)]} {
	      set oldval $actionvarnames($actionname)
	      set actionvarnames($actionname) [list [lindex $oldval 0] [lindex $actionvar 1]]
	    }
	  }
	}
      }
    }
  }

  cgi_body BGCOLOR="$_wp(bordercolor)" {

    cgi_form $_wp(appdir)/wp method=get name=filtconfig target=_top {
      cgi_text "page=conf_process" type=hidden notab
      cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {
	cgi_table_row {
	  #
	  # next comes the menu down the left side
	  #
	  eval {
	    cgi_table_data $_wp(menuargs) rowspan=4 {
	      WPTFCommandMenu filtedit_menu {}
	    }
	  }

	  #
	  # In main body of screen goes confg list
	  #
	  cgi_table_data valign=top width="100%" class=dialog {
	    cgi_text "cp_op=filtconfig" type=hidden notab
	    cgi_text "cid=$cid" type=hidden notab
	    cgi_text "oncancel=$oncancel" type=hidden notab
	    if {[string length $onfiltcancel]} {
	      cgi_text "onfiltcancel=$onfiltcancel" type=hidden notab
	    }
	    cgi_text "fno=$fno" type=hidden notab
	    cgi_text "subop=[expr {$add ? "add" : "edit"}]" type=hidden notab
	    cgi_table border=0 cellspacing=0 cellpadding=0 {
	      # pattern title
	      cgi_table_row {
		cgi_table_data {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"font-weight:bold;font-size:bigger\">Filter Identification</legend>"
		  cgi_table {
		    cgi_table_row {
		      wpGetVarAs nickname filtname
		      freetext_cell "Filter Nickname" nickname $filtname
		    }
		    cgi_table_row {
		      wpGetVarAs comment filtcomment
		      freetext_cell "Comment" comment $filtcomment
		    }
		  }
		  cgi_puts "</fieldset>"
		}
	      }

	      # Folder Conditions
	      wpGetVarAs folder folder
	      wpGetVarAs ftype ftype
	      cgi_table_row {
		cgi_table_data colspan=2 {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"font-weight:bold;font-size:bigger\">Folder Conditions</legend>"
		  cgi_table {
		    cgi_table_row {
		      cgi_table_data align=right valign=top {
			cgi_puts [cgi_bold "Current Folder Type :"]
		      }
		      cgi_table_data align=left {
			cgi_table border=0 cellpadding=0 cellspacing=0 {
			  cgi_table_row {
			    cgi_table_data width=50 {
			      cgi_puts "[cgi_nbspace]"
			    }
			    cgi_table_data {
			      cgi_table border=0 cellpadding=0 cellspacing=0 {
				foreach type {any news email specific} {
				  cgi_table_row {
				    cgi_table_data {
				      cgi_radio_button ftype=$type [expr {[string compare $ftype $type] == 0 ? "checked" : ""}] style="background-color:$_wp(dialogcolor)"
				    }
				    cgi_table_data {
				      switch -- $type {
					any -
					news -
					email {
					  cgi_puts "[string toupper [string range $type 0 0]][string range $type 1 end]"
					}
					specific {
					  cgi_puts "Specific Folder List :"
					  cgi_text "folder=$folder"
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
		  }
		  cgi_puts "</fieldset>"
		}
	      }

	      # Message Conditions

	      cgi_table_row {
		cgi_table_data {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"font-weight:bold;font-size:bigger\">Message Conditions</legend>"
		  cgi_table {
		    foreach {pvarname parvarval} $patvarnamesl {
		      set pvarval [lindex $patvarnames($pvarname) 1]
		      if {$filterr} {
			wpGetVarAs $pvarname pvarval
		      }
		      cgi_table_row {
			switch -- [lindex $patvarnames($pvarname) 2] {
			  freetext {
			    freetext_cell [lindex $patvarnames($pvarname) 0] $pvarname $pvarval
			  }
			  status {
			    cgi_table_data align=right {
			      cgi_puts "[cgi_bold [lindex $patvarnames($pvarname) 0]] :[cgi_nbspace][cgi_nbspace]"
			    }
			    cgi_table_data align=left {
			      cgi_select $pvarname "style=margin:2" {
				cgi_option "Don't care, always matches" "value=either" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
				cgi_option "Yes" "value=yes" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
				cgi_option "No" "value=no" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
			      }
			    }
			  }
			  addrbook {
			    cgi_table_data align=right valign=top {
			      cgi_puts "[cgi_bold [lindex $patvarnames($pvarname) 0]] :[cgi_nbspace][cgi_nbspace]"
			    }
			    cgi_table_data align=left {
			      cgi_select addrbook "style=margin:2" {
				cgi_option "Don't care, always matches" "value=either" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
				cgi_option "Yes, in any address book" "value=yes" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
				cgi_option "No, not in any addressbook" "value=no" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
				cgi_option "Yes, in specific address book" "value=yesspecific" [expr {[string compare $pvarval "yesspecific"] == 0 ? "selected" : ""}]
				cgi_option "No, not in specific address book" "value=nospecific" [expr {[string compare $pvarval "nospecific"] == 0 ? "selected" : ""}]
			      }
			      cgi_br
			      cgi_puts "Specific Addressbook:"
			      cgi_text "specificabook=" "style=margin:4"
			      cgi_br
			      cgi_puts "Types of addresses to check for in address book:"
			      cgi_table style=margin-left:30 {
				cgi_table_row {
				  cgi_table_data nowrap {
				    cgi_checkbox abookfrom [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
				    cgi_puts "From"
				  }

				  cgi_table_data nowrap {
				    cgi_checkbox abookreplyto
				    cgi_puts "Reply-To"
				  }
				}
				cgi_table_row {
				  cgi_table_data nowrap {
				    cgi_checkbox abooksender
				    cgi_puts "Sender"
				  }

				  cgi_table_data nowrap {
				    cgi_checkbox abookto
				    cgi_puts "To"
				  }
				}
				cgi_table_row {
				  cgi_table_data nowrap {
				    cgi_checkbox abookcc
				    cgi_puts "Cc"
				  }
				}
			      }
			    }
			  }
			  headers {
			    cgi_table_data align=right {
			      cgi_put "[cgi_bold [lindex $patvarnames($pvarname) 0]] :[cgi_nbspace][cgi_nbspace]"
			    }
			    cgi_table_data align=left {
			      cgi_submit_button "headers=Add Header"
			    }

			    if {[llength $pvarval] > 0} {
			      cgi_table_data align=left {
				for {set n 0} {$n < [llength $pvarval]} {incr n} {
				  cgi_text "hdrfld$n=[lindex [lindex $pvarval $n] 0]" "style=margin:2"
				  cgi_put ":"
				  cgi_text "hdrval$n=[lindex [lindex $pvarval $n] 1]" "style=margin:2"
				  cgi_br
				}

			      }
			    }
			  }
			  interval {
			    cgi_table_data align=right {
			      cgi_put "[cgi_bold [lindex $patvarnames($pvarname) 0]] :[cgi_nbspace][cgi_nbspace]"
			    }
			    cgi_table_data align=left {
			      for {set n 0} {$n < [llength $pvarval]} {incr n} {
				cgi_text "hdrfld$n=[lindex [lindex $pvarval $n] 0]" sise=4 "style=margin:2"
				cgi_put [cgi_span style=margin:4 "to"]
				cgi_text "hdrval$n=[lindex [lindex $pvarval $n] 1]" sise=4 "style=margin:2"
			      }

			      cgi_text "hdrfld$n=" size=4 "style=margin:2"
			      cgi_put [cgi_span style=margin:4 "to"]
			      cgi_text "hdrval$n=" size=4 "style=margin:2"
			      cgi_submit_button "interval=Another Interval"
			    }
			  }
			}
		      }
		    }
		  }
		  cgi_puts "</fieldset>"
		}
	      }

	      cgi_table_row {
		cgi_table_data {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"font-weight:bold;font-size:bigger\">Filter Actions</legend>"
		  foreach avarname $actionvarnamesl {
		    switch -- $avarname {
		      folder {
			set killit [lindex $actionvarnames(kill) 1]
			if {$filterr} {
			  wpGetVarAs action tval
			  set killit [expr {([string compare $tval "delete"] == 0) ? 1 : 0}]
			}
			cgi_table border=0 cellpadding=0 cellspacing=0 {
			  cgi_table_row {
			    cgi_table_data width=50 align=right valign=top {
			      cgi_puts [cgi_bold "Action:[cgi_nbspace]"]
			    }
			    cgi_table_data {
			      cgi_table border=0 cellpadding=0 cellspacing=0 {
				cgi_table_row {
				  cgi_table_data valign=top {
				    cgi_radio_button action=status [expr {$killit ? "checked" : ""}] style="background-color:$_wp(dialogcolor)"
				  }
				  cgi_table_data {
				    cgi_table {
				      cgi_table_row {
					cgi_table_data valign=top rowspan=4 {
					  cgi_puts "Set Message Status:"
					}
					cgi_table_data {
					  cgi_select actsetimp {
					    cgi_option "Don't change Important Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					    cgi_option "Set Important status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					    cgi_option "Clear Important status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
					  }
					}
				      }
				      cgi_table_row {
					cgi_table_data {
					  cgi_select actsetnew {
					    cgi_option "Don't change New Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					    cgi_option "Set New status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					    cgi_option "Clear New status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
					  }
					}
				      }
				      cgi_table_row {
					cgi_table_data {
					  cgi_select actsetdel {
					    cgi_option "Don't change Deleted Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					    cgi_option "Set Deleted status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					    cgi_option "Clear Deleted status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
					  }
					}
				      }
				      cgi_table_row {
					cgi_table_data {
					  cgi_select actsetans {
					    cgi_option "Don't change Answered Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					    cgi_option "Set Answered status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					    cgi_option "Clear Answered status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
					  }
					}
				      }
				    }
				  }
				}
				cgi_table_row {
				  cgi_table_data valign=top {
				    cgi_radio_button action=delete [expr {$killit ? "checked" : ""}] style="background-color:$_wp(dialogcolor)"
				  }
				  cgi_table_data {
				    cgi_puts "Delete"
				  }
				}
				cgi_table_row {
				  cgi_table_data valign=top {
				    cgi_radio_button action=move  [expr {$killit == 0 ? "checked" : ""}] style="background-color:$_wp(dialogcolor)"
				  }
				  cgi_table_data {
				    cgi_puts "Move to Folder :"
				    if {$filterr} {
				      wpGetVarAs actionfolder tval
				      cgi_text "actionfolder=$tval"
				    } else {
				      cgi_text "actionfolder=[lindex $actionvarnames($avarname) 1]"
				    }
				    cgi_br
				    set tval [expr {[lindex $actionvarnames($avarname) 1] == 1 ? "checked" : ""}]
				    if {$filterr} {
				      wpGetVarAs moind moinval
				      set tval [expr {([string compare $moinval on] == 0) ? "checked" : ""}]
				    }
				    cgi_checkbox moind $tval
				    cgi_puts "Move only if not deleted."
				  }
				}
			      }
			    }
			  }
			  cgi_table_row {
			    wpGetVarAs setkeywords setkeywords
			    freetext_cell "Set these Keywoards" setkeywords $setkeywords
			  }
			  cgi_table_row {
			    wpGetVarAs clearkeywords clearkeywords
			    freetext_cell "Clear these Keywoards" clearkeywords $clearkeywords
			  }
			}
		      }
		    }
		  }

		  cgi_puts "</fieldset>"
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

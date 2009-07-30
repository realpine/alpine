#!./tclsh
# $Id: filtedit.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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
# include common filter info
source filter.tcl

#
#  Input: 
set filtedit_vars {
  {cid     "No cid"}
  {oncancel "No oncancel"}
  {onfiltcancel {} ""}
  {fno     {}  -1}
  {add     {}  0}
  {filterrtext {}  ""}
  {filtedit_score {}  0}
  {filtedit_indexcolor {}  0}
  {fg {}  ""}
  {bg {}  ""}
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

if {$filtedit_score} {
  set filttype score
  set filttypename Score
} elseif {$filtedit_indexcolor} {
  set filttype indexcolor
  set filttypename "Index Color"
} else {
  set filttype filt
  set filttypename Filter
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
	cgi_submit_button "${filttype}_save=Save"
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
	cgi_submit_button "${filttype}help=Get Help"
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


array set idvarnames $pattern_id
array set idvarvals {}
array set patvarnames $pattern_fields
array set patvarvals {}
array set actionvarnames $pattern_actions
array set colvarnames $pattern_colors
array set scorevarnames $pattern_scores

array set actionvals {}

cgi_http_head {
  cgi_content_type
  cgi_pragma no-cache
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "$filttypename List Configuration"
    WPStyleSheets
  }

  if {$add == 0} {
    if {$filtedit_score} {
      set actions $pattern_scores
      set fext [WPCmd PEConfig scoreextended $fno]
    } elseif {$filtedit_indexcolor} {
      set actions $pattern_colors
      set fext [WPCmd PEConfig indexcolorextended $fno]
    } else {
      set actions $pattern_actions
      set fext [WPCmd PEConfig filtextended $fno]
    }

    foreach fvar $fext {
      switch -- [lindex $fvar 0] {
	id {
	  foreach idvar [lindex $fvar 1] {
	    set idname [lindex $idvar 0]
	    if {[info exists idvarnames($idname)]} {
	      set idvarvals($idname) [lindex $idvar 1]
	    }
	  }
	}
	pattern {
	  foreach patternvar [lindex $fvar 1] {
	    set patname [lindex $patternvar 0]
	    if {[info exists patvarnames($patname)]} {
	      set patvarvals($patname) [lindex $patternvar 1]
	    }
	  }
	}
	filtaction {
	  foreach actionvar [lindex $fvar 1] {
	    set actionname [lindex $actionvar 0]
	    if {[info exists actionvarnames($actionname)]} {
	      set actionvals($actionname) [lindex $actionvar 1]
	    }
	  }
	}
	indexcolors {
	  foreach colvar [lindex $fvar 1] {
	    set colname [lindex $colvar 0]
	    if {[info exists colvarnames($colname)]} {
	      set actionvals($colname) [lindex $colvar 1]
	    }
	  }
	}
	scores {
	  foreach colvar [lindex $fvar 1] {
	    set actionvals([lindex $colvar 0]) [lindex $colvar 1]
	  }
	}
      }
    }
  } else {
    if {$filtedit_score} {
      set actions $pattern_scores
    } elseif {$filtedit_indexcolor} {
      set actions $pattern_colors
    } else {
      set actions $pattern_actions
    }
  }

  cgi_body BGCOLOR="$_wp(bordercolor)" {
    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=get name=filtconfig target=_top {
      cgi_text "page=conf_process" type=hidden notab
      cgi_text "cp_op=${filttype}config" type=hidden notab
      cgi_text "cid=$cid" type=hidden notab
      cgi_text "oncancel=$oncancel" type=hidden notab
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
		  cgi_puts "<legend style=\"font-weight:bold;font-size:bigger\">$filttypename Identification</legend>"
		  cgi_table {
		    foreach {idname idtype} $pattern_id {
		      if {[info exists idvarvals($idname)]} {
			set val $idvarvals($idname)
		      } else {
			wpGetVarAs $idname val
		      }
		      cgi_table_row {
			freetext_cell "$filttypename [lindex $idtype 0]" $idname $val
		      }
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
		  cgi_table border=0 {
		    foreach {pvarname parvarval} $pattern_fields {

		      if {$filterr} {
			wpGetVarAs $pvarname pvarval
		      } elseif {[info exists patvarvals($pvarname)]} {
			set pvarval $patvarvals($pvarname)
		      } else {
			set pvarval ""
		      }

		      cgi_table_row {
			switch -- [lindex $patvarnames($pvarname) 1] {
			  freetext {
			    freetext_cell [lindex $patvarnames($pvarname) 0] $pvarname $pvarval
			  }
			  status {
			    cgi_table_data align=right {
			      cgi_puts "[cgi_bold [lindex $patvarnames($pvarname) 0]] :[cgi_nbspace][cgi_nbspace]"
			    }
			    cgi_table_data align=left {
			      cgi_select $pvarname "style=margin:2" {
				cgi_option "Don't care, always matches" "value=either"
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
			    cgi_table_data align=right valign=top {
			      cgi_put "[cgi_bold [lindex $patvarnames($pvarname) 0]] :[cgi_nbspace][cgi_nbspace]"
			    }
			    cgi_table_data align=left {
			      cgi_table {
				set hdrnum 0

				if {[llength $pvarval] > 0} {
				  for {set n 0} {$n < [llength $pvarval]} {incr n} {
				    cgi_table_row {
				      cgi_table_data align=left nowrap {
					cgi_text "hdrfld${n}=[lindex [lindex $pvarval $n] 0]" "style=margin:2"
					cgi_put ":"
					cgi_text "hdrval${n}=[lindex [lindex $pvarval $n] 1]" "style=margin:2"
					cgi_submit_button "rmheader${n}=Remove"
				      }
				    }
				  }

				  cgi_text "header_total=$n" type=hidden notab
				}

				cgi_table_row {
				  cgi_table_data align=left nowrap {
				    cgi_submit_button "addheader=Add Header"
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

	      cgi_table_row {
		cgi_table_data "style=\"padding-bottom: 40\"" {
		  cgi_puts "<fieldset>"
		  cgi_puts "<legend style=\"font-weight:bold;font-size:bigger\">Filter Actions</legend>"
		  foreach {avarname patval} $actions {
		    switch -- $avarname {
		      indexcolor {
			set ih [WPIndexLineHeight]
			set iformat [WPCmd PEMailbox indexformat]
			set num 0
			cgi_division "width=100%" align=center "style=\"font-size: bigger; font-weight: bold; margin: 0 0 12 0 \"" {
			  cgi_puts "Choose Index Line Colors"
			}

			cgi_table width=100% align=center cellpadding=0 cellspacing=0 "style=\"font-family: geneva, arial, sans-serif; height: ${ih}pix; width: 90%; background-color: white ; border: 1px solid black\"" {
			  foreach l [list "Line One" "Sample Message" "Line Three"] {
			    set iclass [lindex {i0 i1} [expr ([incr num] % 2)]]
			    set istyle ""
			    if {$num == 2} {
			      wpGetVarAs fg fg
			      if {[string length $fg] == 0} {
				if {[info exists actionvals($avarname)]} {
				  set fg [lindex $actionvals($avarname) 0]
				}
			      }

			      cgi_text "fg=$fg" type=hidden notab
			      append istyle "; color: $fg"

			      wpGetVarAs bg bg
			      if {[string length $bg] == 0} {
				if {[info exists actionvals($avarname)]} {
				  set bg [lindex $actionvals($avarname) 1]
				}
			      }

			      cgi_text "bg=$bg" type=hidden notab
			      append istyle "; background-color: $bg"
			    }

			    cgi_table_row {
			      if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
				cgi_table_data height=${ih}pix class=$iclass "style=\"$istyle\"" {
				  cgi_checkbox bogus
				}
			      }
			      
			      foreach fmt $iformat {
				cgi_table_data height=${ih}pix width=[lindex $fmt 1]% nowrap class=$iclass "style=\"$istyle\"" {
				  switch -regex [string tolower [lindex $fmt 0]] {
				    number {
				      cgi_puts "$num"
				    }
				    status {
				      set n [expr {(int((10 * rand()))) % 5}]
				      cgi_puts [lindex {N D F A { }} $n]
				    }
				    .*size.* {
				      cgi_puts "([expr int((10000 * rand()))])"
				    }
				    from.* {
				      cgi_puts "Some Sender"
				    }
				    subject {
				      cgi_puts $l
				    }
				    date {
				      cgi_puts [clock format [clock seconds] -format "%d %b"]
				    }
				    default {
				      cgi_puts [lindex $fmt 0]
				    }
				  }
				}
			      }
			    }
			  }
			}
			cgi_table width=80% align=center {
			  cgi_table_row {
			    cgi_table_data align=center {
			      cgi_table "style=\"background-color: #ffcc66\"" {
				wpGetVarAs fgorbg fgorbg
				cgi_table_row {
				  cgi_table_data {
				    if {[string length $fgorbg] == 0 || [string compare $fgorbg fg] == 0} {
				      set checked checked=1
				    } else {
				      set checked ""
				    }

				    cgi_radio_button fgorbg=fg $checked
				  }
				  cgi_table_data "style=\"align: left; padding-left: 12\"" {
				    cgi_puts "Foreground"
				  }
				}
				cgi_table_row {
				  cgi_table_data {
				    if {[string length $checked]} {
				      set checked ""
				    } else {
				      set checked checked=1
				    }

				    cgi_radio_button fgorbg=bg $checked
				  }
				  cgi_table_data "style=\"align: left; padding-left: 12\"" {
				    cgi_puts "Background"
				  }
				}
			      }
			    }
			    cgi_table_data align=center {
			      cgi_image_button "colormap=[WPimg nondither10x10]" alt="Color Pattern" "style=\"border: 1px solid black\""
			    }
			  }
			}
		      }
		      folder {
			if {[info exists actionvals(kill)]} {
			  set killit $actionvals(kill)
			} else {
			  set killit 0
			}

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
				    cgi_puts "Set Message Status:"
				    cgi_division "style=\"margin-left: .5in\"" {
				      cgi_select actsetimp {
					cgi_option "Don't change Important Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					cgi_option "Set Important status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					cgi_option "Clear Important status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
				      }

				      cgi_br

				      cgi_select actsetnew {
					cgi_option "Don't change New Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					cgi_option "Set New status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					cgi_option "Clear New status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
				      }

				      cgi_br

				      cgi_select actsetdel {
					cgi_option "Don't change Deleted Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					cgi_option "Set Deleted status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					cgi_option "Clear Deleted status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
				      }

				      cgi_br

				      cgi_select actsetans {
					cgi_option "Don't change Answered Status" "value=leave" [expr {[string compare $pvarval "either"] == 0 ? "selected" : ""}]
					cgi_option "Set Answered status" "value=set" [expr {[string compare $pvarval "yes"] == 0 ? "selected" : ""}]
					cgi_option "Clear Answered status" "value=clear" [expr {[string compare $pvarval "no"] == 0 ? "selected" : ""}]
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
				      if {[info exists actionvals($avarname)]} {
					set av $actionvals($avarname)
				      } else {
					set av 0
				      }

				      cgi_text "actionfolder=$av"
				    }
				    cgi_br
				    if {$filterr} {
				      wpGetVarAs moind moinval
				      set tval [expr {([string compare $moinval on] == 0) ? "checked" : ""}]
				    } else {
				      if {[info exists actionvals($avarname)] && $actionvals($avarname) == 1} {
					set tval checked
				      } else {
					set tval ""
				      }
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
		      scores {
			cgi_table border=0 cellpadding=4 cellspacing=0 align=center {
			  cgi_table_row {
			    wpGetVarAs scoreval scoreval
			    if {[string length $scoreval] == 0} {
			      set scoreval $actionvals(scoreval)
			    }

			    freetext_cell "Score Value" scoreval $scoreval
			  }
			  cgi_table_row {
			    wpGetVarAs scorehdr scorehdr
			    if {[string length $scorehdr] == 0} {
			      set scorehdr $actionvals(scorehdr)
			    }

			    freetext_cell "Score Header" scorehdr $scorehdr
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

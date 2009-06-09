#!./tclsh

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

set patvarnamesl {nickname comment to from sender cc recip partic news subj alltext stat_new stat_del stat_imp stat_ans ftype folder}
array set patvarnames {
    nickname {"Nickname" "Filter Rule"}
    comment {"Comment" ""}
    to {"To" ""}
    from {"From" ""}
    sender {"Sender" ""}
    cc {"Cc" ""}
    recip {"Recipients" ""}
    partic {"Participants" ""}
    news {"Newsgroups" ""}
    subj {"Subject" ""}
    alltext {"All Text" ""}
    stat_new {"Message is New" "either"}
    stat_del {"Message is Deleted" "either"}
    stat_imp {"Message is Important" "either"}
    stat_ans {"Message is Answered" "either"}
    ftype {"Filter Type" "email"}
    folder {"Folder" "INBOX"}
}

set actionvarnamesl {kill folder move_only_if_not_deleted}
array set actionvarnames {
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
	  # In main body of screen goe confg list
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
	      foreach pvarname $patvarnamesl {
		set pvarval [lindex $patvarnames($pvarname) 1]
		if {$filterr} {
		  wpGetVarAs $pvarname pvarval
		}
		cgi_table_row {
		  switch -- $pvarname {
		    nickname -
		    to -
		    from -
		    sender -
		    cc -
		    recip -
		    partic -
		    news -
		    subj -
		    alltext {
		      cgi_table_data align=right {
			cgi_puts [cgi_bold "[lindex $patvarnames($pvarname) 0] :[cgi_nbspace][cgi_nbspace]"]
		      }
		      cgi_table_data align=left {
			cgi_text "$pvarname=$pvarval" "style=margin:2"
		      }
		    }
		    stat_new -
		    stat_del -
		    stat_imp -
		    stat_ans {
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
		    folder {
		      set folderval $pvarval
		    }
		    ftype {
		      set ftype $pvarval
		    }
		  }
		}
	      }
	    }
	    cgi_p
	    cgi_puts [cgi_bold "Current Folder Type :"]
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
			      cgi_text "folder=$folderval"
			    }
			  }
			}
		      }
		    }
		  }
		}
	      }
	    }
	    cgi_p
	    foreach avarname $actionvarnamesl {
	      switch -- $avarname {
		folder {
		  set killit [lindex $actionvarnames(kill) 1]
		  if {$filterr} {
		    wpGetVarAs action tval
		    set killit [expr {([string compare $tval "delete"] == 0) ? 1 : 0}]
		  }
		  cgi_puts [cgi_bold "Filter Action :"]
		  cgi_table border=0 cellpadding=0 cellspacing=0 {
		    cgi_table_row {
		      cgi_table_data width=50 {
			cgi_puts "[cgi_nbspace]"
		      }
		      cgi_table_data {
			cgi_table border=0 cellpadding=0 cellspacing=0 {
			  cgi_table_row {
			    cgi_table_data {
			      cgi_radio_button action=delete [expr {$killit ? "checked" : ""}] style="background-color:$_wp(dialogcolor)"
			    }
			    cgi_table_data {
			      cgi_puts "Delete"
			    }
			  }
			  cgi_table_row {
			    cgi_table_data {
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
			    }
			  }
			}
		      }
		    }
		  }
		}
		move_only_if_not_deleted {
		  cgi_p
		  cgi_puts "Move only if not deleted :"
		  set tval [expr {[lindex $actionvarnames($avarname) 1] == 1 ? "checked" : ""}]
		  if {$filterr} {
		    wpGetVarAs moind moinval
		    set tval [expr {([string compare $moinval on] == 0) ? "checked" : ""}]
		  }
		  cgi_checkbox moind $tval
		  cgi_p
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

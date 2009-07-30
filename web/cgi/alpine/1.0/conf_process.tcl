#!./tclsh
# $Id: conf_process.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  conf_process.tcl
#
#  Purpose:  CGI script to perform various message/mailbox
#	     oriented operations

source genvars.tcl
source filter.tcl

set cfs_vars {
  {cid		"Missing Command ID"}
  {oncancel     "Missing oncancel"}
  {cp_op       {}      noop}
  {save		{}	0}
  {delete	{}	0}
  {compose	{}	0}
  {cancel	{}	0}
  {gtab         {}      0}
  {mltab        {}      0}
  {mvtab        {}      0}
  {ctab         {}      0}
  {abtab        {}      0}
  {ftab         {}      0}
  {rtab         {}      0}
  {wv           {}      ""}
  {varlistadd   {}      ""}
  {newconf      {}      0}
}

## read vars
foreach item $cfs_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      error [list _action "Import Variable" $result]
    }
  } else {
    set [lindex $item 0] 1
  }
}

proc wpGetVar {_var {valid ""}} {
  upvar $_var var

  if {[catch {cgi_import_as $_var var} result]} {
    error [list _action "Import Var  $_var" $result]
  }

  if {[string length $valid]} {
    switch -exact -- $valid {
      _INTEGER_ {
	if {[string is integer -strict $var] != 1} {
	  error [list _action "Invalid Input" "Non-Numeric Value for $_var"]
	}
      }
      default {
	if {[lsearch -exact $valid $var] < 0} {
	  error [list _action "Invalid Input" "Unrecognized Value $var for $_var"]
	}
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

if {$cid != [WPCmd PEInfo key]} {
  catch {WPCmd PEInfo statmsg "Invalid Command ID"}
}

proc wpGetGoodVars {} {
  global wv
  global general_vars msglist_vars composer_vars folder_vars address_vars msgview_vars rule_vars

  switch -- $wv {
    msgl {
      set goodvars $msglist_vars
    }
    msgv {
      set goodvars $msgview_vars
    }
    address {
      set goodvars $address_vars
    }
    composer {
      set goodvars $composer_vars
    }
    folder {
      set goodvars $folder_vars
    }
    rule {
      set goodvars $rule_vars
    }
    general {
      set goodvars $general_vars
    }
  }
  return $goodvars
}

proc fieldPos {fmt field} {
  for {set i 0} {$i < [llength $fmt]} {incr i} {
    if {[string compare [string toupper [lindex [lindex $fmt $i] 0]] [string toupper $field]] == 0} {
      return $i
    }
  }

  return -1
}

proc numberedVar {nvbase nvtotal} {
  if {[catch {wpGetVarAs $nvtotal nvtot}] == 0} {
    for {set i 0} {$i < $nvtot} {incr i} {
      if {[catch {wpGetVar ${nvbase}${i}} nvval] == 0} {
	return $i
      }
    }
  }

  return -1
}

set op $cp_op
if {[catch {WPCmd set conf_page} conftype]} {
  set conftype general
}
if {[string length $wv]} {
  set conftype $wv
  set op tab
}
if {$save == 1 || [string compare $save Save] == 0} {
  set op tab
  set subop save
} elseif {$newconf} {
  set op noop
} elseif {$gtab} {
  set op "tab"
  set conftype "general"
} elseif {$mltab} {
  set op "tab"
  set conftype "msgl"
} elseif {$mvtab} {
  set op "tab"
  set conftype "msgv"
} elseif {$ctab} {
  set op "tab"
  set conftype "composer"
} elseif {$abtab} {
  set op "tab"
  set conftype "address"
} elseif {$ftab} {
  set op "tab"
  set conftype "folder"
} elseif {$rtab} {
  set op "tab"
  set conftype "rule"
} elseif {$cancel == 1 || [string compare $cancel Cancel] == 0} {
  set op "cancel"
}

proc wpGetRulePattern {} {
    global pattern_fields

    set patlist {}

    foreach {patvar patfield} $pattern_fields {
      wpGetVarAs $patvar tval

      switch $patvar {
	headers {
	  # collect header fields/values into "headers"
	  set headers {}
	  if {[catch {wpGetVarAs header_total hcnt} res] == 0} {
	    for {set i 0} {$i < $hcnt} {incr i} {
	      if {[catch {wpGetVarAs hdrfld${i} fld}] == 0
		  && [catch {wpGetVarAs hdrval${i} val}] == 0} {
		lappend headers [list $fld $val]
	      }
	    }
	  }

	  lappend patlist [list headers $headers]
	}
	default {
	  lappend patlist [list $patvar $tval]
	}
      }
    }


    return $patlist

}

proc wpGetRuleAction {tosave} {
    set actlist {}
    wpGetVar action
    if {$tosave == 1} {
      lappend actlist [list "action" $action]
    } else {
      switch -- $action {
	move {lappend actlist [list "kill" 0]}
	delete {lappend actlist [list "kill" 1]}
      }
    }
    wpGetVar actionfolder
    lappend actlist [list "folder" $actionfolder]
    wpGetVarAs moind moind
    if {[string compare $moind "on"] == 0} {
      lappend actlist [list [expr {$tosave == 1 ? "moind" : "move_only_if_not_deleted"}] "1"]
    } else {
      lappend actlist [list [expr {$tosave == 1 ? "moind" : "move_only_if_not_deleted"}] "0"]
    }
}

	#
	# Meat and potatoes of processing goes on here.
	# Errors are barfed up as they occur,
	# otherwise the result is communicated below...
        #
        set setfeatures [WPCmd PEConfig featuresettings]
        set script fr_tconfig.tcl
	switch -- $op {
	    tab {
	      if {[info exists goodvars] == 0} {
		set goodvars [wpGetGoodVars]
	      }
	      foreach goodvar $goodvars {
		set vtypeinp [lindex $goodvar 0]
		set varname [lindex $goodvar 1]
		set hlpthisvar 0
		wpGetVarAs hlp.$varname.x thlp
		if {[string length $thlp]} {
		  set hlpthisvar 1
		  set helpcancelset conf_process
		}
		switch -- $vtypeinp {
		  special {
		    switch -- $varname {
		      wp-columns {
			if {$hlpthisvar} {
			  set subop varhelp
			  set varhelpname wp-columns
			} else {
			  wpGetVar columns
			  WPCmd PEConfig columns $columns
			}
		      }
		      left-column-folders {
			if {0 == [catch {wpGetVar fcachel}]} {
			  if {$fcachel <= $_wp(fldr_cache_max)} {
			    catch {WPSessionState left_column_folders $fcachel}
			  }
			}
		      }
		      signature {
			wpGetVar signature
			set cursig [string trimright [join [WPCmd PEConfig rawsig] "\n"]]
			set signature [string trimright $signature]
			if {[string compare $cursig $signature]} {
			  WPCmd PEConfig rawsig [split $signature "\n"]
			}
		      }
		      filters {
			wpGetVarAs $varname-sz sz
			wpGetVarAs vla.$varname.x fltadd
			wpGetVarAs hlp.$varname.x do_help
			if {[string length $do_help]} {
			  set subop varhelp
			  set varhelpname filtconf
			} elseif {[string length $fltadd]} {
			  set script "fr_filtedit.tcl"
			  set filtedit_add 1
			  set filtedit_onfiltcancel conf_process
			} else {
			  if {[string length $sz] == 0} {
			    error [list _action "ERROR" "No size given for filters"]
			  }
			  for {set i 0} {$i < $sz} {incr i} {
			    wpGetVarAs vle.$varname.$i.x vle
			    wpGetVarAs vld.$varname.$i.x vld
			    wpGetVarAs vlsu.$varname.$i.x vlsu
			    wpGetVarAs vlsd.$varname.$i.x vlsd
			    set flt_ret 0
			    set flt_res ""
			    if {[string length $vle]} {
			      set script "fr_filtedit.tcl"
			      set filtedit_fno $i
			      set filtedit_onfiltcancel conf_process
			    } elseif {[string length $vld]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset filter delete $i} flt_res]
			    } elseif {[string length $vlsu]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset filter shuffup $i} flt_res]
			    } elseif {[string length $vlsd]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset filter shuffdown $i} flt_res]
			    }
			    if {$flt_ret} {
			      # error
			    } elseif {[string length $flt_res]} {
			      # something wrong here
			    }
			  }
			}
		      }
		      scores {
			wpGetVarAs $varname-sz sz
			wpGetVarAs vla.$varname.x fltadd
			wpGetVarAs hlp.$varname.x do_help
			if {[string length $do_help]} {
			  set subop varhelp
			  set varhelpname filtconf
			} elseif {[string length $fltadd]} {
			  set script "fr_filtedit.tcl"
			  set filtedit_add 1
			  set filtedit_score 1
			  set filtedit_onfiltcancel conf_process
			} else {
			  if {[string length $sz] == 0} {
			    error [list _action "ERROR" "No size given for scores"]
			  }
			  for {set i 0} {$i < $sz} {incr i} {
			    wpGetVarAs vle.$varname.$i.x vle
			    wpGetVarAs vld.$varname.$i.x vld
			    wpGetVarAs vlsu.$varname.$i.x vlsu
			    wpGetVarAs vlsd.$varname.$i.x vlsd
			    set flt_ret 0
			    set flt_res ""
			    if {[string length $vle]} {
			      set script "fr_filtedit.tcl"
			      set filtedit_score 1
			      set filtedit_fno $i
			      set filtedit_onfiltcancel conf_process
			    } elseif {[string length $vld]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset score delete $i} flt_res]
			    } elseif {[string length $vlsu]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset score shuffup $i} flt_res]
			    } elseif {[string length $vlsd]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset score shuffdown $i} flt_res]
			    }
			    if {$flt_ret} {
			      # error
			    } elseif {[string length $flt_res]} {
			      # something wrong here
			    }
			  }
			}
		      }
		      indexcolor {
			wpGetVarAs $varname-sz sz
			wpGetVarAs vla.$varname.x fltadd
			wpGetVarAs hlp.$varname.x do_help
			if {[string length $do_help]} {
			  set subop varhelp
			  set varhelpname filtconf
			} elseif {[string length $fltadd]} {
			  set script "fr_filtedit.tcl"
			  set filtedit_add 1
			  set filtedit_indexcolor 1
			  set filtedit_onfiltcancel conf_process
			} else {
			  if {[string length $sz] == 0} {
			    error [list _action "ERROR" "No size given for index colors"]
			  }
			  for {set i 0} {$i < $sz} {incr i} {
			    wpGetVarAs vle.$varname.$i.x vle
			    wpGetVarAs vld.$varname.$i.x vld
			    wpGetVarAs vlsu.$varname.$i.x vlsu
			    wpGetVarAs vlsd.$varname.$i.x vlsd
			    set flt_ret 0
			    set flt_res ""
			    if {[string length $vle]} {
			      set script "fr_filtedit.tcl"
			      set filtedit_indexcolor 1
			      set filtedit_fno $i
			      set filtedit_onfiltcancel conf_process
			    } elseif {[string length $vld]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset indexcolor delete $i} flt_res]
			    } elseif {[string length $vlsu]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset indexcolor shuffup $i} flt_res]
			    } elseif {[string length $vlsd]} {
			      set flt_ret [catch {WPCmd PEConfig ruleset indexcolor shuffdown $i} flt_res]
			    }
			    if {$flt_ret} {
			      # error
			    } elseif {[string length $flt_res]} {
			      # something wrong here
			    }
			  }
			}
		      }
		      collections {
			wpGetVarAs $varname-sz sz
			wpGetVarAs vla.$varname.x cladd
			if {[string length $cladd]} {
			  set script "fr_cledit.tcl"
			  set cledit_add 1
			  set cledit_onclecancel conf_process
			} else {
			  if {[string length $sz] == 0} {
			    error [list _action "ERROR" "No size given for collections"]
			  }
			  for {set i 0} {$i < $sz} {incr i} {
			    wpGetVarAs vle.$varname.$i.x vle
			    wpGetVarAs vld.$varname.$i.x vld
			    wpGetVarAs vlsu.$varname.$i.x vlsu
			    wpGetVarAs vlsd.$varname.$i.x vlsd
			    set cle_ret 0
			    set cle_res ""
			    if {[string length $vle]} {
			      set script "fr_cledit.tcl"
			      set cledit_cl $i
			      set cledit_onclecancel conf_process
			    } elseif {[string length $vld]} {
			      set cle_ret [catch {WPCmd PEConfig cldel $i} cle_res]
			    } elseif {[string length $vlsu]} {
			      set cle_ret [catch {WPCmd PEConfig clshuff up $i} cle_res]
			    } elseif {[string length $vlsd]} {
			      set cle_ret [catch {WPCmd PEConfig clshuff down $i} cle_res]
			    }
			    if {$cle_ret} {
			      # error
			    } elseif {[string length $cle_res]} {
			      WPCmd PEInfo statmsg $cle_res
			      # something wrong here
			    }
			  }
			}
		      }
		      index-format {
			wpGetVarAs index-format iformat

			set varchanged 0

			if {$hlpthisvar} {
			  set subop varhelp
			  set varhelpname index-format
			} elseif {[catch {cgi_import hlp.index_tokens.x} result] == 0} {
			  set subop secthelp
			  set topicclass plain
			  set feathelpname h_index_tokens
			  set varhelpname h_index_tokens
			}

			if {[catch {cgi_import indexadd}] == 0
			    && [string compare "Add Field" $indexadd] == 0
			    && [catch {cgi_import indexaddfield}] == 0} {
			  if {[lsearch $iformat $indexaddfield] < 0} {
			    set iformat [linsert $iformat 0 $indexaddfield]
			    set varchanged 1
			  }
			} elseif {[catch {cgi_import adjust}] == 0
				   && [string compare Change $adjust] == 0
				   && [catch {cgi_import iop}] == 0
				   && [catch {cgi_import ifield}] == 0
				   && [set pos [fieldPos $iformat $ifield]] >= 0} {
			  switch $iop {
			    left {
			      set iformat [lreplace $iformat $pos $pos]
			      set iformat [linsert $iformat [incr pos -1] $ifield]
			      set varchanged 1
			    }
			    right {
			      set iformat [lreplace $iformat $pos $pos]
			      set iformat [linsert $iformat [incr pos] $ifield]
			      set varchanged 1
			    }
			    widen {
			      set f [lindex [lindex $iformat $pos] 0]
			      set w [lindex [lindex $iformat $pos] 1]
			      set dw [expr {round((100/[llength $iformat]) * [WPTFIndexWidthRatio $iformat $f])}]

			      if {[regexp {([0123456789]+)[%]} $w dummy w] == 0} {
				set w $dw
			      }

			      if {$w < 95} {
				incr w 5
			      } else {
				set w 99
			      }

			      if {$w == $dw} {
				set ws ""
			      } else {
				set ws "${w}%"
			      }

			      set iformat [lreplace $iformat $pos $pos [list $f $ws]]
			      set varchanged 1
			    }
			    narrow {
			      set f [lindex [lindex $iformat $pos] 0]
			      set w [lindex [lindex $iformat $pos] 1]
			      set dw [expr {round((100/[llength $iformat]) * [WPTFIndexWidthRatio $iformat $f])}]

			      if {[regexp {([0123456789]+)[%]} $w dummy w] == 0} {
				set w $dw
			      }

			      if {$w > 5} {
				incr w -5
			      } else {
				set w 1
			      }

			      if {$w == $dw} {
				set ws ""
			      } else {
				set ws "${w}%"
			      }

			      set iformat [lreplace $iformat $pos $pos [list $f $ws]]
			      set varchanged 1
			    }
			    remove {
			      set iformat [lreplace $iformat $pos $pos]
			      set varchanged 1
			    }
			  }
			} else {
			  foreach f $iformat {
			    if {[catch {cgi_import_as shrm.${f}.x shift} result] == 0} {
			      if {[set pos [fieldPos $iformat $f]] >= 0} {
				set iformat [lreplace $iformat $pos $pos]
				set varchanged 1
			      }
			    } elseif {[catch {cgi_import_as shlf.${f}.x shift} result] == 0} {
			      if {[set pos [fieldPos $iformat $f]] > 0} {
				set iformat [lreplace $iformat $pos $pos]
				set iformat [linsert $iformat [incr pos -1] $f]
				set varchanged 1
			      }
			    } elseif {[catch {cgi_import_as shrt.${f}.x shift} result] == 0} {
			      if {[set pos [fieldPos $iformat $f]] >= 0} {
				set iformat [lreplace $iformat $pos $pos]
				set iformat [linsert $iformat [incr pos] $f]
				set varchanged 1
			      }
			    }
			  }
			}

			if {$varchanged} {
			  foreach f $iformat {
			    if {[string length [lindex $f 1]]} {
			      lappend ifv "[lindex $f 0]([lindex $f 1])"
			    } else {
			      lappend ifv [lindex $f 0]
			    }
			  }

			  WPCmd PEConfig varset index-format [list $ifv]
			}
		      }
		      view-colors {
			if {$hlpthisvar} {
			  set subop varhelp
			  set varhelpname index-format
			} elseif {[catch {cgi_import_as colormap.x colx}] == 0
				  && [catch {cgi_import_as colormap.y coly}] == 0} {
			  set rgbs {"000" "051" "102" "153" "204" "255"}
			  set xrgbs {"00" "33" "66" "99" "CC" "FF"}
			  set rgblen [llength $rgbs]
			  set imappixwidth 10

			  set colx [expr {${colx} / $imappixwidth}]
			  set coly [expr {${coly} / $imappixwidth}]
			  if {($coly >= 0 && $coly < $rgblen)
			      && ($colx >= 0 && $colx < [expr {$rgblen * $rgblen}])} {
			    set ired $coly
			    set igreen [expr {($colx / $rgblen) % $rgblen}]
			    set iblue [expr {$colx % $rgblen}]
			    set rgb "[lindex $rgbs $ired],[lindex $rgbs ${igreen}],[lindex $rgbs ${iblue}]"
			    set xrgb "[lindex $xrgbs $ired][lindex $xrgbs ${igreen}][lindex $xrgbs ${iblue}]"

			    if {[catch {cgi_import_as text tt}] == 0} {
			      set type [split $tt .]
			      catch {WPCmd set config_deftext [lindex $type end]}

			      if {[catch {cgi_import_as ground ground}] == 0} {
				switch $ground {
				  f {
				    catch {WPCmd set config_defground f}
				    set fg $xrgb
				  }
				  b {
				    catch {WPCmd set config_defground b}
				    set bg $xrgb
				  }
				}

				if {[info exists fg] || [info exists bg]} {
				  switch [lindex $type 0] {
				    hdr {
				      set type [lindex $type 1]
				      if {[catch {cgi_import_as add.${type} foo}] == 0} {
					set colop add
				      } elseif {[catch {cgi_import_as hi.${type} hindex}] == 0} {
					set colop change
				      }

				      if {[info exists colop]} {
					if {![info exists bg] && [catch {cgi_import_as dbg.$type bg} result]} {
					  WPCmd PEInfo statmsg "Can't import default background: $result"
					} elseif {![info exists fg] && [catch {cgi_import_as dfg.$type fg} result]} {
					  WPCmd PEInfo statmsg "Can't import default foreground: $result"
					}

					switch $colop {
					  change {
					    if {[catch {WPCmd PEConfig colorset viewer-hdr-colors update [list $hindex $type ""] [list $fg $bg]} result]} {
					      WPCmd PEInfo statmsg "Problem changing $type color: $result"
					    }
					  }
					  add {
					    if {[catch {WPCmd PEConfig colorset viewer-hdr-colors add [list $type ""] [list $fg $bg]} result]} {
					      WPCmd PEInfo statmsg "Problem adding $type color: $result"
					    }
					  }
					}
 
				      }
				    }
				    default {
				      if {![info exists bg] && [catch {cgi_import_as dbg.$type bg} result]} {
					WPCmd PEInfo statmsg "Can't import default background: $result"
				      } elseif {![info exists fg] && [catch {cgi_import_as dfg.$type fg} result]} {
					WPCmd PEInfo statmsg "Can't import default foreground: $result"
				      } elseif {[catch {WPCmd PEConfig colorset $type [list $fg $bg]} result]} {
					WPCmd PEInfo statmsg "Can't set $type color: $result"
				      }
				    }
				  }
				} else {
				    WPCmd PEInfo statmsg "Invalid fore/back ground input!"
				}
			      } else {
				WPCmd PEInfo statmsg "Choose foreground or background!"
			      }
			    } else {
			      WPCmd PEInfo statmsg "Choose the type of text to color!"
			    }
			  } else {
			    WPCmd PEInfo statmsg "Invalid RGB Input!"
			  }
			} elseif {[catch {cgi_import addfield}] == 0
				  && [string compare "add " [string tolower [string range $addfield 0 3]]] == 0
				  && [catch {cgi_import newfield}] == 0
				  && [string length [set newfield [string trim $newfield]]]
				  && [catch {cgi_import_as dfg.normal dfg}] == 0
				  && [catch {cgi_import_as dbg.normal dbg}] == 0} {
			  if {[catch {WPCmd PEConfig colorset viewer-hdr-colors add [list $newfield ""] [list $dfg $dbg]} result]} {
			    WPCmd PEInfo statmsg "Problem adding $type color: $result"
			  }
			} elseif {[catch {cgi_import reset}] == 0
				  && [string compare "restore " [string tolower [string range $reset 0 7]]] == 0} {
			  if {[catch {cgi_import_as text tt}] == 0} {
			    if {[llength [set type [split $tt .]]] == 2 && [string compare [lindex $type 0] hdr] == 0} {
			      set hdr [lindex $type end]
			      if {[catch {cgi_import_as hi.$hdr hindex}] == 0} {
				if {[catch {WPCmd PEConfig colorset viewer-hdr-colors delete $hindex} result]} {
				  # bug: reloads cause this error - need better way to report it
				  #WPCmd PEInfo statmsg "Can't reset $hdr ($hindex) text: $result!"
				} else {
				  catch {WPCmd PEInfo unset config_deftext}
				}
			      }
			    } elseif {[string compare normal $tt] == 0} {
			      if {[catch {WPCmd PEConfig varset normal-foreground-color ""} result]
				  || [catch {WPCmd PEConfig varset normal-background-color ""} result]} {
				WPCmd PEInfo statmsg "Can't reset normal text: $result!"
			      }
			    } elseif {[catch {cgi_import_as dfg.normal dfg}] == 0
				      && [catch {cgi_import_as dbg.normal dbg}] == 0} {
			      catch {WPCmd set config_deftext $tt}
			      if {[catch {WPCmd PEConfig colorset $tt [list $dfg $dbg]} result]} {
				WPCmd PEInfo statmsg "Can't reset $tt text: $result!"
			      }
			    }
			  } else {
			    WPCmd PEInfo statmsg "Choose the type of text to color!"
			  }
			}
		      }
		    }
		  }
		  var {
		    wpGetVarAs $varname formval
		    set varvals [WPCmd PEConfig varget $varname]
		    set vals [lindex $varvals 0]
		    set vartype [lindex $varvals 1]
		    set formvals [split $formval "\n"]
		    set varchanged 0

		    if {$hlpthisvar} {
		      set subop varhelp
		      set varhelpname $varname
		    }

		    if {[string compare $vartype textarea] == 0} {
		      wpGetVarAs vla.$varname.x vlavar
		      wpGetVarAs $varname-sz sz
		      wpGetVarAs $varname-add valadd
		      if {[string length $vlavar]} {
			set fr_tconfig_vlavar $varname
		      }
		      set formvals {}
		      if {[string length $valadd]} {
			lappend formvals $valadd
		      }
		      if {[string length $sz]} {
			set prevwassd 0
			for {set i 0} {$i < $sz} {incr i} {
			  wpGetVarAs vle.$varname.$i fval
			  wpGetVarAs vld.$varname.$i.x fvaldel
			  wpGetVarAs vlsu.$varname.$i.x fvalsu
			  wpGetVarAs vlsd.$varname.$i.x fvalsd
			  set fed 0
			  set fdel 0
			  set fsu 0
			  set fsd 0
			  if {[string length $fval]} {
			    set fed 1
			  }
			  if {[string length $fvaldel]} {
			    set fdel 1
			  } elseif {[string length $fvalsu]} {
			    set fsu 1
			  } elseif {[string length $fvalsd]} {
			    set fsd 1
			  }
			  if {$fed && $fdel == 0 && $prevwassd} {
			    set prevwassd 0
			    set formvals [linsert $formvals [expr {[llength $formvals] - 1}] $fval]
			  } elseif {$fed && $fdel == 0 && $fsu == 0} {
			    lappend formvals $fval
			    if {$fsd} {
			      set prevwassd 1
			    }
			  } elseif {$fed && $fdel == 0 && $fsu} {
			    set fvallen [llength $formvals]
			    if {$fvallen} {
			      set formvals [linsert $formvals [expr {$fvallen - 2}] $fval]
			    } else {
			      lappend formvals $fval
			    }
			  }
			}
		      }
		      set len [llength $formvals]
		      if {$len != [llength $vals]} {
			set varchanged 1
		      } else {
			for {set i 0} {$i < $len} {incr i} {
			  if {[string compare [lindex $formvals $i] [lindex $vals $i]]} {
			    set varchanged 1
			    break
			  }
			}
		      }
		    } elseif {[llength $formvals] != [llength $vals]} {
		      set varchanged 1
		    } else {
		      set valslength [llength $vals]
		      for {set i 0} {$i < $valslength} {incr i} {
			if {[string compare [lindex $vals $i] [lindex $formvals $i]]} {
			  set varchanged 1
			  break
			}
		      }
		    }
		    if {$varchanged} {
		      WPCmd PEConfig varset $varname $formvals
		    }
		    # what about wp-indexheight?
		  }
		  feat {
		    wpGetVarAs $varname tval
		    if {$hlpthisvar} {
		      set subop feathelp
		      set feathelpname $varname
		    }
		    set featset [expr {[lsearch $setfeatures $varname] >= 0}]
		    set formfeatset [expr {[string compare $tval on] == 0}]
		    if {$formfeatset != $featset} {
		      WPCmd PEConfig feature $varname $formfeatset
		    }
		  }
		}
	      }
	      if {[info exists subop]} {
		switch -- $subop {
		  varhelp {
		    catch {WPCmd PEInfo unset help_context}
		    catch {WPCmd set oncancel $oncancel}
		    set help_vars [list topic topicclass]
		    set topic $varhelpname
		    set _cgi_uservar(topic) $varhelpname
		    set topicclass variable
		    set _cgi_uservar(topicclass) variable
		    set _cgi_uservar(oncancel) conf_process
		    set script help
		  }
		  feathelp {
		    catch {WPCmd PEInfo unset help_context}
		    catch {WPCmd set oncancel $oncancel}
		    set help_vars [list topic topicclass oncancel]
		    set topic $feathelpname
		    set _cgi_uservar(topic) $feathelpname
		    set topicclass feature
		    set _cgi_uservar(topicclass) feature
		    set _cgi_uservar(oncancel) conf_process
		    set script help
		  }
		  secthelp {
		    catch {WPCmd PEInfo unset help_context}
		    catch {WPCmd set oncancel $oncancel}
		    set help_vars [list topic topicclass oncancel]
		    set topic $feathelpname
		    set _cgi_uservar(topic) $feathelpname
		    set topicclass $topicclass
		    set _cgi_uservar(topicclass) $topicclass
		    set _cgi_uservar(oncancel) conf_process
		    set script help
		  }
		  save {
		    if {$cid != [WPCmd PEInfo key]} {
		      error [list _close "Invalid Operation ID"]
		    }
		    WPCmd PEConfig saveconf
		    set script $oncancel
		    catch {WPCmd PEInfo unset config_deftext}
		  }
		}
	      }
	    }
	    filtconfig {
	      wpGetVar fno [list _INTEGER_]
	      wpGetVar subop [list edit add]

	      if {[catch {wpGetVar filtcancel}]} {
		if {[catch {wpGetVar filthelp}] == 0} {
		  catch {WPCmd PEInfo unset help_context}
		  catch {WPCmd set oncancel $oncancel}

		  set patlist [wpGetRulePattern]
		  set actlist [wpGetRuleAction 0]
		  # we have to save this exactly as it would look when getting it from alpined
		  set ftsadd [expr {[string compare $subop "add"] == 0 ? 1 : 0}]
		  set ftsform [list [list "pattern" $patlist] [list "filtaction" $actlist]]
		  catch {WPCmd set filttmpstate [list $ftsadd $fno $ftsform]}

		  set help_vars [list topic]
		  set topic filtedit
		  set _cgi_uservar(topic) filtedit

		  if {[string compare $subop "edit"] == 0} {
		    set fakeimg "vle.filters.$fno"
		    set fakesz [expr {$fno + 1}]
		  } else {
		    set fakeimg "vla.filters"
		    set fakesz 1
		  }

		  set _cgi_uservar(oncancel) [WPPercentQuote "conf_process&wv=rule&filters-sz=${fakesz}&${fakeimg}.x=1&${fakeimg}.y=1&oncancel=main.tcl"]
		  set script help
		} elseif {[set nv [numberedVar rmheader header_total]] >= 0} {

		  # load all the rules, process "headers"
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		    
		    if {[string compare headers [lindex $pat 0]] == 0} {
		      if {[llength $headers] > $nv} {
			set headers [lreplace $headers $nv $nv]
		      }
		    }
		  }

		  # load all the actions
		  foreach act [wpGetRuleAction 0] {
		    set [lindex $act 0] [lindex $act 1]
		  }

		  # load other variables
		  wpGetVarAs nickname nickname
		  wpGetVarAs comment comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set filterrtext 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} elseif {[catch {wpGetVar addheader}] == 0} {

		  # load all the rules, process "headers"
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		    if {[string compare headers [lindex $pat 0]] == 0} {
		      foreach h [set headers [lindex $pat 1]] {
			if {0 == [string length [lindex $h 0]]
			    && 0 == [string length [lindex $h 1]]} {
			  set emptyheader 1
			}
		      }
		      
		      if {![info exists emptyheader]} {
			lappend headers [list {} {}]
		      }
		    }
		  }

		  # load all the actions
		  foreach act [wpGetRuleAction 0] {
		    set [lindex $act 0] [lindex $act 1]
		  }

		  # load other variables
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set filterrtext 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} else {
		  # load other variables
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set patlist [wpGetRulePattern]
		  set actlist [wpGetRuleAction 1]

		  lappend patlist [list nickname $nickname]
		  lappend patlist [list comment $comment]

		  set ret [catch {WPCmd PEConfig ruleset filter $subop $fno $patlist $actlist} res]
		  if {$ret} {
		    error [list _action "Filter Set" $res]
		  } elseif {[string length $res]} {
		    WPCmd PEInfo statmsg "Filter setting failed: $res"

		    set filtedit_fno $fno
		    set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		    set filtedit_onfiltcancel conf_process
		    set script "fr_filtedit.tcl"
		  }
		}
	      }
	    }
	    scoreconfig {
	      wpGetVar fno [list _INTEGER_]
	      wpGetVar subop [list edit add]

	      if {[catch {wpGetVar filtcancel}]} {
		if {[catch {wpGetVar filthelp}] == 0} {
		  catch {WPCmd PEInfo unset help_context}
		  catch {WPCmd set oncancel $oncancel}
		  if {[string compare $subop "edit"] == 0 || [string compare $subop "add"] == 0} {
		    set patlist [wpGetRulePattern]

		    # we have to save this exactly as it would look when getting it from alpined
		    set ftsadd [expr {[string compare $subop "add"] == 0 ? 1 : 0}]
		    set ftsform [list [list "pattern" $patlist] [list "filtaction" $actlist]]
		    catch {WPCmd set filttmpstate [list $ftsadd $fno $ftsform]}
		  }
		  set help_vars [list topic]
		  set topic scoreedit
		  set _cgi_uservar(topic) scoreedit
		  switch -- $subop {
		    edit {
		      set fakeimg "vle.scores.$fno"
		      set fakesz [expr {$fno + 1}]
		    }
		    add  {
		      set fakeimg "vla.scores"
		      set fakesz 1
		    }
		  }

		  set _cgi_uservar(oncancel) [WPPercentQuote "conf_process&wv=rule&scores-sz=${fakesz}&${fakeimg}.x=1&${fakeimg}.y=1&oncancel=main.tcl"]
		  set script help
		} elseif {[set nv [numberedVar rmheader header_total]] >= 0} {

		  # load all the rules, process "headers"
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		    
		    if {[string compare headers [lindex $pat 0]] == 0} {
		      if {[llength $headers] > $nv} {
			set headers [lreplace $headers $nv $nv]
		      }
		    }
		  }

		  # load other variables
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set filterrtext 1
		  set filtedit_score 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} elseif {[catch {wpGetVar addheader}] == 0} {

		  # load all the rules, process "headers"
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		    if {[string compare headers [lindex $pat 0]] == 0} {
		      foreach h [set headers [lindex $pat 1]] {
			if {0 == [string length [lindex $h 0]]
			    && 0 == [string length [lindex $h 1]]} {
			  set emptyheader 1
			}
		      }
		      
		      if {![info exists emptyheader]} {
			lappend headers [list {} {}]
		      }
		    }
		  }

		  # load other variables
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set filterrtext 1
		  set filtedit_score 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} else {
		  switch -- $subop {
		    edit -
		    add {
		      # load other variables
		      wpGetVar nickname
		      wpGetVar comment
		      wpGetVarAs folder folder
		      wpGetVarAs ftype ftype

		      set patlist [wpGetRulePattern]

		      lappend patlist [list nickname $nickname]
		      lappend patlist [list comment $comment]

		      wpGetVar scoreval
		      lappend actlist [list "scoreval" $scoreval]

		      wpGetVar scorehdr
		      lappend actlist [list "scorehdr" $scorehdr]

		      set ret [catch {WPCmd PEConfig ruleset score $subop $fno $patlist $actlist} res]

		      if {$ret} {
			error [list _action "Score Set" $res]
		      } elseif {[string length $res]} {
			WPCmd PEInfo statmsg "Score setting failed: $res"

			set filtedit_score 1
			set filtedit_fno $fno
			set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
			set filtedit_onfiltcancel conf_process
			set script "fr_filtedit.tcl"
		      }
		    }
		  }
		}
	      }
	    }
	    indexcolorconfig {
	      wpGetVar fno [list _INTEGER_]
	      wpGetVar subop [list edit add]

	      if {[catch {wpGetVar filtcancel}]} {
		if {[catch {wpGetVar filthelp}] == 0} {
		  catch {WPCmd PEInfo unset help_context}
		  catch {WPCmd set oncancel $oncancel}
		  if {[string compare $subop "edit"] == 0 || [string compare $subop "add"] == 0} {
		    set patlist [wpGetRulePattern]

		    # we have to save this exactly as it would look when getting it from alpined
		    set ftsadd [expr {[string compare $subop "add"] == 0 ? 1 : 0}]
		    set ftsform [list [list "pattern" $patlist] [list "filtaction" $actlist]]
		    catch {WPCmd set filttmpstate [list $ftsadd $fno $ftsform]}
		  }
		  set help_vars [list topic]
		  set topic indexcoloredit
		  set _cgi_uservar(topic) indexcoloredit
		  switch -- $subop {
		    edit {
		      set fakeimg "vle.indexcolor.$fno"
		      set fakesz [expr {$fno + 1}]
		    }
		    add  {
		      set fakeimg "vla.indexcolor"
		      set fakesz 1
		    }
		  }

		  set _cgi_uservar(oncancel) [WPPercentQuote "conf_process&wv=rule&indexcolor-sz=${fakesz}&${fakeimg}.x=1&${fakeimg}.y=1&oncancel=main.tcl"]
		  set script help
		} elseif {[set nv [numberedVar rmheader header_total]] >= 0} {

		  # load all the rules, process "headers"
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		    
		    if {[string compare headers [lindex $pat 0]] == 0} {
		      if {[llength $headers] > $nv} {
			set headers [lreplace $headers $nv $nv]
		      }
		    }
		  }

		  # load other variables
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set filterrtext 1
		  set filtedit_indexcolor 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} elseif {[catch {wpGetVar addheader}] == 0} {
		  # load all the rules, process "headers"
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		    if {[string compare headers [lindex $pat 0]] == 0} {
		      foreach h [set headers [lindex $pat 1]] {
			if {0 == [string length [lindex $h 0]]
			    && 0 == [string length [lindex $h 1]]} {
			  set emptyheader 1
			}
		      }
		      
		      if {![info exists emptyheader]} {
			lappend headers [list {} {}]
		      }
		    }
		  }

		  # load other variables
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype

		  set filterrtext 1
		  set filtedit_indexcolor 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} elseif {[catch {cgi_import_as colormap.x colx}] == 0
			  && [catch {cgi_import_as colormap.y coly}] == 0} {
		  set rgbs {"000" "051" "102" "153" "204" "255"}
		  set xrgbs {"00" "33" "66" "99" "CC" "FF"}
		  set rgblen [llength $rgbs]
		  set imappixwidth 10

		  set colx [expr {${colx} / $imappixwidth}]
		  set coly [expr {${coly} / $imappixwidth}]
		  if {($coly >= 0 && $coly < $rgblen)
		      && ($colx >= 0 && $colx < [expr {$rgblen * $rgblen}])} {
		    set ired $coly
		    set igreen [expr {($colx / $rgblen) % $rgblen}]
		    set iblue [expr {$colx % $rgblen}]
		    set rgb "[lindex $rgbs $ired],[lindex $rgbs ${igreen}],[lindex $rgbs ${iblue}]"
		    set xrgb "[lindex $xrgbs $ired][lindex $xrgbs ${igreen}][lindex $xrgbs ${iblue}]"

		    if {[catch {wpGetVar fgorbg [list fg bg]}]} {
		      WPCmd PEInfo statmsg "Invalid fore/back ground input!"
		      catch {unset xrgb}
		    }
		  } else {
		    WPCmd PEInfo statmsg "Invalid RGB Input!"
		  }

		  # relay any other config changes
		  wpGetVar nickname
		  wpGetVar comment
		  wpGetVarAs folder folder
		  wpGetVarAs ftype ftype
		  foreach pat [wpGetRulePattern] {
		    set [lindex $pat 0] [lindex $pat 1]
		  }

		  # import previous settings
		  wpGetVarAs fg fg
		  wpGetVarAs bg bg

		  # set new value
		  if {[info exists xrgb]} {
		    set $fgorbg $xrgb
		  }

		  set filterrtext 1
		  set filtedit_indexcolor 1
		  set filtedit_fno $fno
		  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
		  set filtedit_onfiltcancel conf_process
		  set script "fr_filtedit.tcl"
		} else {
		  switch -- $subop {
		    edit -
		    add {

		      wpGetVar nickname
		      wpGetVar comment

		      set patlist [wpGetRulePattern]

		      lappend patlist [list nickname $nickname]
		      lappend patlist [list comment $comment]

		      # save config?
		      set actlist {}
		      if {[catch {wpGetVar fg}] == 0 && [catch {wpGetVar bg}] == 0} {
			lappend actlist [list fg $fg]
			lappend actlist [list bg $bg]

			# save rule
			set ret [catch {WPCmd PEConfig ruleset indexcolor $subop $fno $patlist $actlist} res]
			if {$ret} {
			  error [list _action "Color Set Error" $res]
			} elseif {[string length $res]} {
			  WPCmd PEInfo statmsg "Index Color setting failed: $res"

			  set filtedit_indexcolor 1
			  set filtedit_fno $fno
			  set filtedit_add [expr {[string compare $subop add] == 0 ? 1 : 0}]
			  set filtedit_onfiltcancel conf_process
			  set script "fr_filtedit.tcl"
			}
		      } else {
			error [list _action "Unset FG/BG" "Internal Error: Unset Color Variables"]
		      }
		    }
		  }
		}
	      }
	    }
	    clconfig {
	      wpGetVar cl
	      wpGetVar nick
	      wpGetVar server
	      wpGetVar user
	      wpGetVar stype
	      wpGetVar path
	      wpGetVar view
	      wpGetVar add
	      wpGetVarAs cle_cancel.x cle_cancel
	      wpGetVarAs cle_save.x cle_save

	      set cledit_add $add
	      set cledit_cl $cl
	      set cledit_onclecancel conf_process
	      if {[string length $cle_save]} {
		if {[catch {cgi_import_as "ssl" sslval}]} {
		  set ssl 0
		} else {
		  if {[string compare $sslval on] == 0} {
		    set ssl 1
		  } else {
		    set ssl 0
		  }
		}
		regexp "\{?(\[^\}\]*)\}?(.*)" $server match serverb serverrem
		if {[string length $serverb]} {
		  if {$ssl == 1} {
		    set serverb "$serverb/ssl"
		  }
		  if {[string compare "" "$user"]} {
		    set serverb "$serverb/user=$user"
		  }
		  if {[string compare "imap" [string tolower $stype]]} {
		    set serverb "$serverb/[string tolower $stype]"
		  }
		  if {[string compare "nntp" [string tolower $stype]] == 0} {
		    regsub -nocase {^(#news\.)?(.*)$} "$path" "#news.\\2" path
		    if {[string compare "" $path] == 0} {
		      set path "#news."
		    }
		  }
		  set result ""
		  set ret 0
		  set servera "\{$serverb\}$serverrem"
		  if {$add} {
		    set ret [catch {WPCmd PEConfig cladd $cl $nick $servera $path $view} result]
		  } else {
		    set ret [catch {WPCmd PEConfig cledit $cl $nick $servera $path $view} result]
		  }
		  if {$ret != 0} {
		    error [list _action "Collection List Set" $result]
		  } elseif {[string compare "" $result]} {
		    if {$add} {
		      set clerrtext "Add failed: $result"
		    } else {
		      set clerrtext "Edit failed: $result"
		    }
		    WPCmd PEInfo statmsg $clerrtext
		    set script "fr_cledit.tcl"
		  }
		} else {
		  set clerrtext "Bad data: Nothing defined for Server"
		  WPCmd PEInfo statmsg $clerrtext
		  set script "fr_cledit.tcl"
		}
	      }
	    }
	    noop {
		catch {WPCmd PEInfo noop}
	    }
	    cancel {
	      set script $oncancel
	      catch {WPCmd unset conf_page} res
	    }
	    default {
		error [list _close "Unknown process operation: $op"]
	    }
	}

source [WPTFScript $script]

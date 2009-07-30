#!./tclsh
# $Id: cmdfunc.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  cmdfunc.tcl
#
#  Purpose:  CGI script to serve as single location for menu/command
#	     function definitions
#
#   OPTIMIZE: have servlet interpreter grok/exec these?
#
#  Input:

#  Output:
#

proc WPTFTitle {{context {some page}} {newmail {}} {nologo 0} {aboutcancel {}}} {
  global _wp

  cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" class=title  {
    cgi_table_row {
      if {!$nologo} {
	cgi_table_data valign=top align=left height=$_wp(titleheight) {

	  if {[string length $aboutcancel]} {
	    cgi_put [cgi_url [cgi_imglink smalllogo] wp.tcl?page=help&topic=about&oncancel=$aboutcancel class=navbar target=_top]
	  } else {
	    cgi_put [cgi_imglink smalllogo]
	  }
	}
      }

      # work in new mail here
      if {[llength $newmail]} {
	cgi_table_data align=center {
	  WPTFStatusTable $newmail
	}
      }

      cgi_table_data align=right valign=middle height=$_wp(titleheight) {
	cgi_put [cgi_span "style=margin-right: 8; color: $_wp(titlecolor)" "$context"]
      }
    }
  }
}

proc WPTFStatusTable {msgs {iconlink {0}} {style {}}} {
  global _wp

  cgi_table width=100% border=0 cellpadding=0 cellspacing=0 $style {
    cgi_table_row align=right {

      if {[info exists _wp(statusicons)] && $_wp(statusicons)} {
	set img [cgi_imglink bang]
	set snd ""
	foreach m $msgs {
	  if {[string length [lindex $m 1]]} {
	    set img [cgi_imglink [lindex $m 1]]
	    if {$iconlink && [string length [lindex $m 2]]} {
	      set img [cgi_url $img wp.tcl?page=view&uid=[lindex $m 2] target=body]
	    }

	    set snd [lindex $m 3]
	    break
	  }
	}

	cgi_table_data {
	  cgi_puts ${img}${snd}
	}
      }

      cgi_table_data align=center class="statustext" {
	set i 0

	foreach m $msgs {

	  if {[array exists lastmsg] && [info exists lastmsg([lindex $m 0])]} {
	    incr lastmsg([lindex $m 0])
	    continue
	  }

	  if {0 == [string compare [string range [lindex $m 0] 0 20] "Alert received while "]} {
	    set style "style=border: 1px solid red ; background-color: pink; padding: 2; width: 80%;"
	  } elseif {!([info exists _wp(statusicons)] && $_wp(statusicons))} {
	    set style "style=color: white ; background-color: $_wp(menucolor); padding-left:8px; padding-right:8px; white-space: nowrap;"
	  } else {
	    set style
	  }

	  if {$iconlink && [string length [lindex $m 2]] && !([info exists _wp(statusicons)] && $_wp(statusicons))} {
	    set txt [cgi_url [lindex $m 0] wp.tcl?page=fr_view&uid=[lindex $m 2] target=body "style=text-decoration: none; color: white"]
	  } else {
	    set txt [lindex $m 0]
	  }

	  cgi_division "style=\"padding-bottom: 1px\"" {
	    cgi_puts [cgi_span $style $txt]
	  }

	  set lastmsg([lindex $m 0]) 1
	}
      }

      if {[info exists _wp(statusicons)] && $_wp(statusicons)} {
	cgi_table_data align=left {
	  cgi_puts $img
	}
      }
    }
  }
}


proc WPTFImageButton {args} {
  return [cgi_buffer {eval cgi_image_button $args border=0}]
}

proc WPTFCommandMenu {s_menu c_menu} {
  global _wp

  set clist {}
  if {[string length $s_menu]} {
    upvar $s_menu specificmenu
    if {[llength $specificmenu]} {
      lappend clist $specificmenu
    }
  }

  if {[string length $c_menu]} {
    upvar $c_menu commonmenu
    if {[llength $commonmenu]} {
      if {[llength $clist]} {
	lappend clist {}
      }
      lappend clist $commonmenu
    }
  }

  cgi_table border=0 bgcolor=$_wp(menucolor) cellpadding=0 cellspacing=0 width=112 "style=\"padding: 8 0 8 4\"" {
    foreach menulist $clist {
      switch [llength $menulist] {
	0 {
	  cgi_table_row {
	    cgi_table_data {
	      cgi_hr "width=75%"
	    }
	  }
	}
	default {
	  foreach item $menulist {
	    if {[llength $item] == 0} {
	      cgi_table_row {
		cgi_table_data {
		  cgi_hr "width=75%"
		}
	      }
	      continue
	    }
	    if {[llength $item] == 1} {
	      cgi_table_row {
		cgi_table_data {
		  eval [lindex $item 0]
		}
	      }
	      continue
	    }
	    if {[string length [lindex $item 0]]} {
	      if {[uplevel [lindex $item 0]] == 0} {
		continue
	      }
	    }

	    cgi_table_row {
	      foreach l [lindex $item 1] {
		cgi_table_data align=left valign=middle class=navbar {
		  uplevel $l
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

proc WPTFScript {scrpt {dflt ""}} {
  global _wp

  switch -- $scrpt {
    main {
      set src main.tcl
    }
    index {
      set src index.tcl
      WPCmd PEInfo set wp_body_script $src
    }
    view {
      set src view.tcl
      WPCmd PEInfo set wp_body_script $src
    }
    body {
      if {[catch {WPCmd PEInfo set wp_body_script} src]} {
	set src index.tcl
	catch {WPCmd PEInfo set wp_body_script $src}
      }
    }
    fr_view {
      set src do_view.tcl
    }
    quit {
      set src fr_queryquit.tcl
    }
    folders {
      set src folders.tcl
    }
    fldrbrowse {
      set src fldrbrowse.tcl
    }
    fldrsavenew {
      set src fldrsavenew.tcl
    }
    fldrdel {
      set src fr_querydelfldr.tcl
    }
    compose {
      set src fr_compose.tcl
    }
    addrbrowse {
      set src fr_addrbrowse.tcl
    }
    savebrowse {
      set src fr_fldrbrowse.tcl
    }
    savecreate {
      set src fr_fldrsavenew.tcl
    }
    take {
      set src fr_take.tcl
    }
    takeedit {
      set src fr_takeedit.tcl
    }
    takesame {
      set src fr_takesame.tcl
    }
    ldapbrowse {
      set src fr_ldapbrowse.tcl
    }
    addrbook {
      set src addrbook.tcl
    }
    tconfig {
      set src tconfig.tcl
    }
    cledit {
      set src cledit.tcl
    }
    filtedit {
      set src filtedit.tcl
    }
    conf_process {
      set src conf_process.tcl
    }
    resume {
      set src fr_resume.tcl
    }
    spell {
      set src fr_spellcheck.tcl
    }
    auth {
      set src fr_queryauth.tcl
    }
    expunge {
      set src fr_queryexpunge.tcl
    }
    askattach {
      set src fr_queryattach.tcl
    }
    ldapquery {
      set src fr_ldapquery.tcl
    }
    querycreate {
      set src fr_querycreate.tcl
    }
    queryprune {
      set src fr_queryprune.tcl
    }
    attach {
      set src attach.tcl
    }
    dosend {
      set src dosend.tcl
    }
    docancel {
      set src docancel.tcl
    }
    help {
      set src fr_help.tcl
    }
    split {
      set src fr_split.tcl
    }
    default {
      if {[regexp {.*\.tcl$} $scrpt s]} {
	set src $scrpt
      } elseif {[string length $dflt]} {
	set src $dflt
      } else {
	error "Unrecognized script abbreviation: $scrpt"
      }
    }
  }

  return [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) $src]
}

proc WPTFSaveDefault {{uid 0}} {
  # "size" rather than "number" to work around temporary alpined bug
  if {$uid == 0
      || [catch {WPCmd PEMessage $uid size} n]
      || $n == 0
      || [catch {WPCmd PEMessage $uid savedefault} savedefault]} {
    if {[WPCmd PEFolder isincoming 0]} {
      set colid 1
    } else {
      set colid 0
    }

    return [list $colid "saved-messages"]
  }

  return $savedefault
}

if {$_wp(keybindings)} {
  proc WPTFKeyEquiv {kl {exclusions {}} {frame window}} {
    if {[llength $kl] > 0} {
      WPStdScripts

      append js "function bindListener(o,f)\{"
      if {[isW3C]} {
	append js "o.addEventListener('keypress',f, false);\n"
	set cancelkeystroke "e.preventDefault(); return false;"
      } elseif {[isIE]} {
	append js  "o.onkeydown = f;\n"
	set cancelkeystroke "return false;"
      } else {
	append js  "o.onkeydown = f;"
	append js  "o.captureEvents(Event.KEYDOWN);\n"
	set cancelkeystroke "return false;"
      }
      append js "\}\n"

      append js "function nobubble(e)\{"
      if {[isW3C]} {
	append js " e.stopPropagation();"
      } elseif {[isIE]} {
	append js " event.cancelBubble = true;"
      }
      append js "\}\n"

      append js  "function keyed(e)\{"
      if {[isW3C] && [llength $exclusions]} {
	set ex ""
	foreach o $exclusions {
	  if {[string length $ex]} {
	    append ex " || "
	  }

	  append ex "e.target == $o"
	}
	append js "if (e.target && ($ex)) return true;"
      }
      append js  " var c = getKeyStr(e);"
      append js  " if(getControlKey(e))\{"
      append js  "  switch(c)\{"
      append js  "   case 'n' : window.status = 'New window creation disabled in WebPine.' ; $cancelkeystroke"
      append js  "  \}"
      append js  " \}"
      append js  " else"
      append js  "  switch(c)\{"
      foreach kb $kl {
	append js  "  case '[lindex $kb 0]' : ${frame}.webpinelink = 1; [lindex $kb 1] ; $cancelkeystroke"
      }

      append js  "  \}\}\n"

      set onload "bindListener(document,keyed);"

      if {![isW3C]} {
	foreach o $exclusions {
	  append onload "bindListener($o,nobubble);"
	}
      }

      cgi_javascript {
	cgi_puts $js
      }

      return $onload
    }
  }
}

# add given folder name to the cache of saved-to folders
proc WPTFAddSaveCache {f_name} {
  global _wp

  if {[catch {WPSessionState save_cache} flist] == 0} {
    if {[set i [lsearch -exact $flist $f_name]] < 0} {
      set flist [lrange $flist 0 [expr {$_wp(save_cache_max) - 2}]]
    } else {
      set flist [lreplace $flist $i $i]
    }

    set flist [linsert $flist 0 $f_name]
  } else {
    set flist [list $f_name]
  }

  catch {WPSessionState save_cache $flist}
}

# return the list of cached saved-to folders and make sure given
# default is somewhere in the list
proc WPTFGetSaveCache {{def_name ""}} {

  if {![string length $def_name]} {
    set savedef [WPTFSaveDefault 0]
    set def_name [lindex $savedef 1]
  }

  set seen ""

  if {[catch {WPSessionState save_cache} flist] == 0} {
    foreach f $flist {
      if {[string compare $def_name $f] == 0} {
	set def_listed 1
      }

      if {[string length $f] && [lsearch -exact $seen $f] < 0} {
	lappend options $f
	lappend options $f
	lappend seen $f
      }
    }
  }

  if {!([info exists options] && [info exists def_listed])} {
    lappend options $def_name
    lappend options $def_name
  }

  if {[catch {WPCmd set wp_cache_folder} wp_cache_folder]
      || [string compare $wp_cache_folder [WPCmd PEMailbox mailboxname]]} {
    # move default to top on new folder
    switch -- [set x [lsearch -exact $options $def_name]] {
      0 { }
      default {
	if {$x > 0} {
	  set options [lreplace $options $x [expr {$x + 1}]]
	}

	set options [linsert $options 0 $def_name]
	set options [linsert $options 0 $def_name]
      }
    }

    catch {WPCmd set wp_cache_folder [WPCmd PEMailbox mailboxname]}
  }

  lappend options "\[ folder I type in \]"
  lappend options "__folder__prompt__"

  lappend options "\[ folder in my folder list \]"
  lappend options "__folder__list__"

  return $options
}

# add given folder name to the visited folder cache
proc WPTFAddFolderCache {f_col f_name} {
  global _wp

  if {$f_col != 0 || [string compare [string tolower $f_name] inbox]} {
    if {0 == [catch {WPSessionState folder_cache} flist]} {

      if {[catch {WPSessionState left_column_folders} fln]} {
	set fln $_wp(fldr_cache_def)
      }

      for {set i 0} {$i < [llength $flist]} {incr i} {
	set f [lindex $flist $i]
	if {$f_col == [lindex $f 0] && 0 == [string compare [lindex $f 1] $f_name]} {
	  break
	}
      }

      if {$i >= [llength $flist]} {
	set flist [lrange $flist 0 $fln]
      } else {
	set flist [lreplace $flist $i $i]
      }

      set flist [linsert $flist 0 [list $f_col $f_name]]
      # let users of data know it's changed (cheaper than hash)
      WPScriptVersion common 1
    } else {
      catch {unset flist}
      lappend flist [list $f_col $f_name] [list [WPTFSaveDefault 0]]
      # ditto
      WPScriptVersion common 1
    }

    catch {WPSessionState folder_cache $flist}
  }
}

# return the list of cached visited folders and make sure given
# default is somewhere in the list
proc WPTFGetFolderCache {} {
  if {[catch {WPSessionState folder_cache} flist]} {
    catch {unset flist}
    lappend flist [WPTFSaveDefault 0]
    catch {WPSessionState folder_cache $flist}
  }

  return $flist
}

proc WPExitOnClose {{frame window}} {
  global _wp 

  cgi_script  type="text/javascript" language="JavaScript" {
    cgi_put  "function wpLink(){"
    cgi_put  " ${frame}.webpinelink = 1;"
    cgi_put  " return true;"
    cgi_puts "}"
    cgi_put  "function wpLoad(){"
    cgi_put  " ${frame}.webpinelink = 0;"
    cgi_puts "}"
    cgi_put  "function wpUnLoad(){"
    cgi_put  " if(!${frame}.webpinelink){"
    cgi_put  "  window.open('[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/ripcord.tcl?t=10&cid=[WPCmd PEInfo key]','Depart','width=350,height=200');"
    cgi_put  " }"
    cgi_puts "}"
  }

  uplevel 1 {

    # tweak some cgi_* procs for global effect
    if {0 == [catch {rename cgi_url _wp_orig_cgi_url}]} {
      proc cgi_url {args} {
	lappend newargs [lindex $args 0]
	foreach a [lrange $args 1 end] {
	  if {[regexp "^(onClick)=(.*)" $a dummy attr str]} {
	    set onclicked 1
	    lappend newargs "${attr}=wpLink();${str}"
	  } else {
	    lappend newargs $a
	  }
	}

	if {![info exists onclicked]} {
	  lappend newargs "onClick=wpLink();"
	}

	return [eval "_wp_orig_cgi_url $newargs"]
      }
    }

    if {0 == [catch {rename cgi_area _wp_orig_cgi_area}]} {
      proc cgi_area {args} {
	lappend newargs [lindex $args 0]
	foreach a [lrange $args 1 end] {
	  if {[regexp "^(onClick)=(.*)" $a dummy attr str]} {
	    set onclicked 1
	    lappend newargs "${attr}=\"wpLink();${str}\""
	  } else {
	    lappend newargs $a
	  }
	}

	if {![info exists onclicked]} {
	  lappend newargs "onClick=\"return wpLink();\""
	}

	return [eval "_wp_orig_cgi_area $newargs"]
      }
    }

    if {0 == [catch {rename cgi_form _wp_orig_cgi_form}]} {
      proc cgi_form {args} {
	foreach a [lrange $args 0 [expr [llength $args]-2]] {
	  if {[regexp "^onSubmit=(.*)" $a dummy str]} {
	    set onsubmitted 1
	    lappend newargs "onSubmit=wpLink(); ${str}"
	  } else {
	    lappend newargs $a
	  }
	}

	if {![info exists onsubmitted]} {
	  lappend newargs "onSubmit=return wpLink();"
	}

	lappend newargs [lindex $args end]

	uplevel 1 "_wp_orig_cgi_form $newargs"
      }
    }
  }
}

proc WPTFIndexWidthRatio {fields field} {
  # should be formula based on total fields
  # and number of "wider" fields
  switch [string toupper $field] {
    TO			-
    FROM		-
    FROMORTO		-
    FROMORTONOTNEWS	-
    RECIPS		-
    SENDER		-
    SUBJECT		{ return 1.25 }
    default		{ return 1 }
  }
}

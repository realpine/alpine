# $Id: folders.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  folders.tcl
#
#  Purpose:  CGI script to generate html output associated with folder
#	     and collection management
#
#  Input:
set folder_vars {
  {uid		""	0}
  {cid		{}	""}
  {show		{}	""}
  {expand	{}	""}
  {contract	{}	""}
  {oncancel	""	main}
  {frestore	""	0}
  {delquery	{}	""}
  {dwnquery	{}	""}
  {delete	{}	""}
  {renquery	{}	""}
  {rename	{}	""}
  {newfolder	{}	""}
  {folder	{}	""}
  {newdir	{}	""}
  {directory	{}	""}
  {import	{}	""}
  {cancelled	{}	""}
  {fid		{}	""}
  {reload}
}

set indention 18

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

proc linecolor {linenum} {
  if {$linenum % 2} {
    return "#EEEEEE"
  } else {
    return "#FFFFFF"
  }
}

proc pretty_folder_name {collections folder} {
  set fcolid [lindex $folder 0]
  for {set i 0} {$i < [llength $collections]} {incr i} {
    set col [lindex $collections $i]
    if {$fcolid == [lindex $col 0]} {
      set coltext [lindex $col 1]
    }
  }    
  return "${coltext}:[join [lrange $folder 1 end] /]"
}

#
# Display the given folder list in a table (w/ mihodge mods)
#
proc blat_folder_list {colid flist shown path baseurl scroll anchorcntref depth} {
  global key border indention mbox _wp

  upvar $anchorcntref anchorcnt

  set rownum 0

  foreach folder $flist {
    set t [lindex $folder 0]
    set f [lindex $folder 1]
    set ff [linsert $path [llength $path] $f]
    set index -1

    set bgcolor [linecolor [incr anchorcnt]]

    # initial pad=12, expand/contract control is 9px wide
    set cellpad [expr {12 + ($depth * $indention)}]
    set delim [WPCmd PEFolder delimiter $colid]
    set fullpath [join [lrange $ff 1 end] $delim]

    if {[string first F $t] >= 0} {
      regsub -all {(')} [lrange $ff 1 end] {\\\\\1} ef
      set celldata [cgi_url $f open.tcl?colid=${colid}&folder=[WPPercentQuote $fullpath]&oncancel=folders&cid=$key target=_top]
    } else {
      set celldata $f
    }

    if {[string first D $t] >= 0} {

      if {[set index [lsearch $shown $ff]] < 0} {
	set control expand
      } else {
	set control contract
      }

      set celldata "[cgi_url [cgi_imglink $control] "${baseurl}${control}=[WPPercentQuote $ff]#f_[WPPercentQuote $ff]" name=f_[WPPercentQuote $ff] "style=\"padding-right:10px\""]$celldata"
      incr cellpad -19
    }

    cgi_table_row bgcolor=$bgcolor {

      cgi_table_data align=center {
	if {[string first F $t] >= 0 || ([WPCmd PEFolder isincoming $colid] == 0 && [string compare $mbox $fullpath])} {
	  cgi_radio_button "fid=f_[WPPercentQuote $ff]"
	}
      }

      cgi_table_data align=left "style=\"padding-left: ${cellpad}px\"" nowrap {
	cgi_put $celldata
      }

      cgi_table_data valign=top nowrap {
	if {[info exists control] && [string compare $control contract] == 0} {
	  cgi_submit_button "new_[WPPercentQuote $ff]=Create New..."
	  cgi_submit_button "imp_[WPPercentQuote $ff]=Import..."
	} else {
	  cgi_puts [cgi_nbspace]
	}
      }
    }

    if {[string first D $t] >= 0 && $index >= 0} {
      set nflist [eval WPCmd PEFolder list $ff]
      set newpath $path
      lappend newpath $f
      blat_folder_list $colid $nflist $shown $newpath $baseurl $scroll anchorcnt [expr {$depth + 1}]
    }

    catch {unset control}
  }
}


#
# Command Menu definition for Message View Screen
#
set folder_menu {
}

set common_menu {
  {
    {}
    {
      {
	# * * * * Ubiquitous INBOX link * * * *
	if {[string compare inbox [string tolower $mbox]]} {
	  cgi_put [cgi_url INBOX [cgi_root]/$_wp(appdir)/$_wp(ui1dir)/open.tcl?folder=INBOX&colid=0&cid=[WPCmd PEInfo key] target=_top class=navbar]
	} else {
	  cgi_put [cgi_url INBOX fr_main.tcl target=_top class=navbar]
	}
      }
    }
  }
  {
    {}
    {
      {
	# * * * * COMPOSE * * * *
	cgi_puts [cgi_url Compose wp.tcl?page=compose&oncancel=folders&cid=$key target=_top class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * RESUME * * * *
	#set button [cgi_img [WPimg but_resume] border=0 alt="Resume"]
	set button Resume
	cgi_puts [cgi_url $button wp.tcl?page=resume&oncancel=folders&cid=$key class=navbar]
      }
    }
  }
  {
    {}
    {
      {
	# * * * * Addr books * * * *
	#set button [cgi_img [WPimg but_abook] border=0 alt="Address Book"]
	set button "Address Book"
	cgi_puts [cgi_url $button wp.tcl?page=addrbook&oncancel=folders class=navbar]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {}
    {
      {
	cgi_put [cgi_url "Configure" wp.tcl?page=conf_process&newconf=1&oncancel=folders&cid=[WPCmd PEInfo key] class=navbar target=_top]
      }
    }
  }
  {
    {}
    {
      {
	cgi_put [cgi_url "Get Help" wp.tcl?page=help&oncancel=folders class=navbar target=_top]
      }
    }
  }
  {{cgi_puts [cgi_nbspace]}}
  {
    {}
    {
      {
	# * * * * LOGOUT * * * *
	if {[WPCmd PEInfo feature quit-without-confirm]} {
	  cgi_puts [cgi_url "Quit $_wp(appname)" $_wp(serverpath)/session/logout.tcl?cid=[WPCmd PEInfo key]&sessid=$sessid target=_top class=navbar]
	} else {
	  cgi_puts [cgi_url "Quit $_wp(appname)" wp.tcl?page=quit&cid=[WPCmd PEInfo key] target=_top class=navbar]
	}
      }
    }
  }
}

## read vars
foreach item $folder_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} errstr]} {
      error [list _action "Impart Variable" $errstr]
    }
  } else {
    set [lindex $item 0] 1
  }
}

if {[catch {WPCmd PEInfo key} key]} {
  error [list _action "command ID" $key]
}

# perform requested op
if {$delquery == 1 || [string compare $delquery Delete] == 0} {
  if {[string length $fid]} {
    set fid [string range $fid 2 end]
    source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_querydelfldr.tcl]
    set nopage 1
  } else {
    lappend newmail [list "Click button next to folder name then Click [cgi_italic Delete]"]
  }
} elseif {$delete == 1 || [string compare $delete Delete] == 0} {
  if {$cid != $key} {
    lappend newmail [list "Invalid Command ID"]
  } elseif {[string length $fid]} {
    if {[catch [concat WPCmd PEFolder delete $fid] result] == 0} {
      lappend newmail [list "'[lindex $fid end]' permanently removed"]
    }
  } else {
    lappend newmail [list "Click button next to folder name then Click [cgi_italic Delete]"]
  }
} elseif {[string compare $delete Cancel] == 0} {
  catch {WPCmd PEInfo unset wp_folder_script}
  lappend newmail [list "Folder Delete Cancelled"]
} elseif {[string compare $rename Cancel] == 0} {
  catch {WPCmd PEInfo unset wp_folder_script}
  lappend newmail [list "Folder Rename Cancelled"]
} elseif {$renquery == 1 || [string compare $renquery Rename] == 0} {
  if {$cid != $key} {
    lappend newmail [list "Invalid Command ID"]
  } elseif {[string length $fid]} {
    set fid [string range $fid 2 end]
    source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_queryrenfldr.tcl]
    set nopage 1
  } else {
    lappend newmail [list "Click button next to folder name then Click [cgi_italic Rename]"]
  }
} elseif {$dwnquery == 1 || [string compare $dwnquery Export] == 0} {
  if {$cid != $key} {
    lappend newmail [list "Invalid Command ID"]
  } elseif {[string length $fid] <= 0} {
    lappend newmail [list "Click button next to folder name then Click [cgi_italic Export]"]
  } elseif {[file isdirectory $_wp(detachpath)] <= 0} {
    lappend newmail [list "Server Configuration Problem: $_wp(detachpath) does not exist"]
  } else {
    source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) exporting.tcl]
    set nopage 1
  }
} elseif {[string compare $rename Rename] == 0} {
  if {[string length $folder]} {
    if {[catch [concat WPCmd PEFolder rename $fid [list $folder]] result]} {
    } else {
      lappend newmail [list "'[lindex $fid end]' renamed to '$folder'"]
    }
  } else {
    lappend newmail [list "Rename failed: no new name provided"]
  }
} elseif {[string compare [string range $newfolder 0 5] Create] == 0} {
  if {$cid != $key} {
    lappend newmail [list "Invalid Command ID"]
  } elseif {[string length $folder]} {
    set fpath [lrange $fid 1 end]
    lappend fpath $folder
    if {[catch {WPCmd PEFolder delimiter [lindex $fid 0]} result]
	|| [catch {WPCmd PEFolder create [lindex $fid 0] [join $fpath $result]} result]} {
      lappend newmail [list "Create failed: $result"]
    } else {
      lappend newmail [list "Folder $folder created"]
    }
  } else {
    lappend newmail [list "Folder creation failed: no folder name provided"]
  }
} elseif {[string compare $newfolder Cancel] == 0} {
  catch {WPCmd PEInfo unset wp_folder_script}
  lappend newmail [list "Folder Create Cancelled"]
} elseif {[string compare [string range $import 0 5] Import] == 0} {
  if {[catch {WPImport file "Missing File Upload"} errstr] == 0} {
    set local_file [lindex $file 0]

    if {[catch {WPImport iname "import name"} errstr] == 0} {
      set iname [string trim $iname]

      if {[string length $iname]} {

	set colid [lindex $fid 0]
	set fldr [eval "file join [lrange $fid 1 end] $iname"]

	if {[catch {WPCmd PEFolder import $local_file $colid $fldr} errstr] == 0} {
	  lappend newmail [list "Imported folder $iname"]
	} else {
	  lappend newmail [list "Can't Import File: $errstr"]
	}
      } else {
	lappend newmail [list "Must provide uploaded folder name"]
      }
    } else {
      lappend newmail [list "Can't get uploaded folder name"]
    }

    catch {file delete -force $local_file}
  } else {
    lappend newmail [list "Problem uploading file"]
  }
} elseif {[string compare [string range $newdir 0 5] Create] == 0} {
  if {$cid != $key} {
    lappend newmail [list "Invalid Command ID"]
  } elseif {[string length $directory]} {
    set fpath [lrange $fid 1 end]
    lappend fpath "${directory}/"
    if {[catch {WPCmd PEFolder delimiter [lindex $fid 0]} result]
	|| [catch {WPCmd PEFolder create [lindex $fid 0] [join $fpath $result]} result]} {
      lappend newmail [list "Create failed: $result"]
    } else {
      lappend newmail [list "Folder $directory created"]
    }
  } else {
    lappend newmail [list "Directory Create failed: no name provided"]
  }
} elseif {[string compare $newdir Cancel] == 0} {
  catch {WPCmd PEInfo unset wp_folder_script}
  lappend newmail [list "Directory Creation Cancelled"]
} elseif {[string compare $cancelled Cancel] == 0} {
  catch {WPCmd PEInfo unset wp_folder_script}
  lappend newmail [list "New Folder or Directory Creation Cancelled"]
} elseif {[catch {WPCmd PEInfo set wp_folder_script} script] == 0} {
  catch {WPCmd PEInfo unset wp_folder_script}
  set uid 0
  source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) $script]
  set nopage 1
} else {
  foreach i [cgi_import_list] {
    switch -regexp -- $i {
      ^new_[a-zA-Z0-9%_]*$ {
	set fid [string range $i 4 end]
	catch {WPCmd PEInfo set fid $fid}
	catch {WPCmd PEInfo set wp_folder_script fr_querynewdir.tcl}
	source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_querynewfoldir.tcl]
	set nopage 1
      }
      ^nd_[a-zA-Z0-9%_]*$ {
	set fid [string range $i 3 end]
	catch {WPCmd PEInfo set fid $fid}
	catch {WPCmd PEInfo set wp_folder_script fr_querynewdir.tcl}
	source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_querynewdir.tcl]
	set nopage 1
      }
      ^nf_[a-zA-Z0-9%_]*$ {
	set fid [string range $i 3 end]
	catch {WPCmd set fid $fid}
	catch {WPCmd set wp_folder_script fr_querynewfldr.tcl}
	source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_querynewfldr.tcl]
	set nopage 1
      }
      ^imp_[a-zA-Z0-9%_]*$ {
	set fid [string range $i 4 end]
	source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_queryimport.tcl]
	set nopage 1
      }
      default {
      }
    }

    catch {WPCmd PEInfo unset fid}
    catch {WPCmd PEInfo unset wp_folder_script}
  }
}

if {[info exists nopage] == 0} {
  if {$reload || $frestore || ([string length $show] == 0 && [string length $expand] == 0 && [string length $contract] == 0)} {
    catch {set show [WPCmd PEInfo set fr_show]
    set expand [WPCmd PEInfo set fr_expand]
    set contract [WPCmd PEInfo set fr_contract]}
  } else {
    WPCmd set fr_show $show
    WPCmd set fr_expand $expand
    WPCmd set fr_contract $contract
  }

  # collect top level folder lists
  if {[catch {WPCmd PEFolder collections} collections]} {
    error [list _action "Collection list" $collections]
  }

  set shown [split $show ,]
  set scroll {}
  set anchorcnt 0

  # mihodge: process actions
  if {[llength $expand]} {
    lappend shown $expand
    set scroll $expand
  }

  if {[llength $contract]} {
    if {[set index [lsearch $shown $contract]] >= 0} {
      set shown [lreplace $shown $index $index]
      set scroll $contract
    }
  }

  set baseurl wp.tcl?page=folders&

  if {[llength $shown]} {
    append baseurl "show=[WPPercentQuote [join $shown ,]]&"
  }

  # build top-level collection's folder list
  for {set i 0} {$i < [llength $collections]} {incr i} {
    set col [lindex $collections $i]
    set colid [lindex $col 0]

    if {[llength $collections] == 1} {
      set flist 1
    } else {
      if {[set index [lsearch $shown $colid]] < 0} {
	set flist {}
      } else {
	set flist 1
      }
    }

    if {[llength $flist]} {
      if {[catch {WPCmd PEFolder list $colid} flist]} {
	if {[string compare BADPASSWD [string range $flist 0 8]] == 0} {
	  # control error messages
	  set statmsgs [WPCmd PEInfo statmsgs]
	  WPCmd PEMailbox newmailreset
	  if {[catch {WPCmd PESession creds [lindex $expand 0] folder} creds] == 0 && $creds != 0} {
	    catch {WPCmd PEInfo statmsg "Invalid Username or Password"}
	    WPCmd PESession nocred $expand folder
	  }

	  if {[catch {WPCmd PEFolder clextended} coln]} {
	    WPCmd set reason "Can't get Collection Info: $coln"
	  } else {
	    set coln [lindex $coln $expand]
	    if {[regexp {^([a-zA-Z\.]+).*\/user=([^ /]*)} [lindex $coln 4] dummy srvname authuser]} {
	      WPCmd set reason "Listing folders in the [cgi_bold [lindex $coln 1]] collection first requires that you log in to the server [cgi_bold "$srvname"]."
	      WPCmd set authuser $authuser
	    } else {
	      WPCmd set reason "Folders in the [cgi_bold [lindex $coln 1]] collection are on a server that must be logged into."
	    }
	  }

	  WPCmd set cid [WPCmd PEInfo key]
	  WPCmd set authcol [lindex $expand 0]
	  WPCmd set authfolder folder
	  WPCmd set authpage [WPPercentQuote "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders&expand=$expand"]
	  WPCmd set authcancel [WPPercentQuote "[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders"]

	  source [file join $_wp(cgipath) $_wp(appdir) $_wp(ui1dir) fr_queryauth.tcl]

	  catch {WPCmd unset fr_expand}

	  set nopage 1
	} else {
	  set flist {}
	}
      }
    }

    lappend collectionfolders $flist
  }
}

if {[info exists nopage] == 0} {
  # collect new mail message and errors
  if {[catch {WPNewMail $reload} newmailmsg]} {
    error [list _action "new mail" $newmailmsg]
  } else {
    foreach m $newmailmsg {
      lappend newmail $m
    }
    
    if {[info exists newmail] == 0} {
      set newmail ""
    }
  }

  # paint the page
  cgi_http_head {
    WPStdHttpHdrs text/html
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Folder List" folders

      set onload "onLoad="
      set onunload "onUnload="
      set normalreload [cgi_buffer {WPHtmlHdrReload "$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders"}]

      if {[info exists _wp(exitonclose)]} {
	WPExitOnClose
	append onload "wpLoad();"
	append onunload "wpUnLoad();"

	cgi_script  type="text/javascript" language="JavaScript" {
	  cgi_put  "function viewReloadTimer(t){"
	  cgi_put  " reloadtimer = window.setInterval('wpLink(); window.location.replace(\\'[cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders&reload=1\\')', t * 1000);"
	  cgi_puts "}"
	}

	append onload "viewReloadTimer($_wp(refresh));"
	cgi_noscript {
	  cgi_puts $normalreload
	}
      } else {
	cgi_puts $normalreload
      }

      WPStyleSheets
      if {$_wp(keybindings)} {
	set kequiv {
	  {{i} {top.location = 'fr_main.tcl'}}
	  {{a} {top.location = 'wp.tcl?page=addrbook'}}
	  {{?} {top.location = 'wp.tcl?page=help&oncancel=folders'}}
	}

	lappend kequiv [list {c} "top.location = 'wp.tcl?page=compose&oncancel=folders&cid=$key'"]

	append onload [WPTFKeyEquiv $kequiv]
      }
    }

    cgi_body bgcolor=$_wp(bordercolor) background=[file join $_wp(imagepath) logo $_wp(logodir) back.gif] "style=\"background-repeat: repeat-x\"" $onload $onunload {

      catch {WPCmd PEInfo set help_context folders}

      # prepare context and navigation information

      set mbox [WPCmd PEMailbox mailboxname]

      lappend pagehier [list "Folder List"]
      lappend pagehier [list [cgi_bold "\[Return to $mbox\]"] fr_main.tcl "View list of messages"]
      if {[string compare $oncancel view] == 0} {
	if {$uid} {
	  set num [WPCmd PEMessage $uid number]
	} else {
	  set num View
	}

	lappend pagehier [list [cgi_bold "\[Return to Message $num\]"] view.tcl "View Message"]
      }

      set navops ""

      WPTFTitle "Folder List" $newmail 0 "folders"

      cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {

	cgi_table_row {
	  cgi_table_data valign=top class=navbar {
	    cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=0 {
	      cgi_table_row {
		cgi_table_data class=navbar "style=\"padding: 6 0 0 4\"" {
		  cgi_puts [cgi_span "style=font-weight: bold" "Current Folder"]
		  cgi_division align=center "style=margin-top:4;margin-bottom:4" {
		    set mbn [WPCmd PEMailbox mailboxname]
		    if {[string length $mbn] > 16} {
		      set mbn "[string range $mbn 0 14]..."
		    }

		    cgi_put [cgi_url $mbn fr_main.tcl target=_top class=navbar]

		    switch -exact -- [WPCmd PEMailbox state] {
		      readonly {
			cgi_br
			#cgi_put [cgi_span "style=color: black; border: 1px solid red; background-color: pink; font-weight: bold" "Read Only"]
			cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Read Only)"]
		      }
		      closed {
			cgi_br
			#cgi_put [cgi_span "style=color: black; border: 1px solid red; background-color: pink; font-weight: bold" "Closed"]
			cgi_put [cgi_span "style=color: pink; font-weight: bold" "(Closed)"]
		      }
		      ok -
		      default {}
		    }

		    cgi_br
		  }

		  cgi_hr "width=75%"
		}
	      }

	      # next comes the menu down the left side, with suitable
	      cgi_table_row {
		eval {
		  cgi_table_data $_wp(menuargs) class=navbar style=padding-bottom:10 {
		    WPTFCommandMenu folder_menu common_menu
		  }
		}
	      }
	    }
	  }

	  # down the right side of the table is the window's contents
	  cgi_table_data width="100%" bgcolor=#ffffff valign=top {

	    if {[llength $collections] > 1} {
	      cgi_division "style=\"font-family: helvetica; padding: 18; text-align: center\"" {
		cgi_puts [cgi_span "style=font-weight: bold; font-size: 14pt; vertical-align: middle" "Folder Collections"]
		cgi_br
		cgi_puts [cgi_span "style=font-size: smaller" "(click to expand, open, delete, etc.)"]
	      }
	    }

	    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post target=_top {
	      cgi_text "page=folders" type=hidden notab
	      cgi_text "cid=$key" type=hidden notab
	      cgi_text "frestore=1" type=hidden notab
	      # then the table representing the folders
	      cgi_table border=0 cellspacing=0 cellpadding=2 align=center {
		for {set i 0} {$i < [llength $collections]} {incr i} {
		  set col [lindex $collections $i]
		  set colid [lindex $col 0]

		  if {[llength $collections] == 1} {
		    set menu "c"
		    set flist 1
		  } else {
		    if {[set index [lsearch $shown $colid]] < 0} {
		      set menu "ce"
		      set flist {}
		    } else {
		      set menu "cc"
		      set flist 1
		    }
		  }

		  cgi_table_row {
		    cgi_table_data nowrap valign=middle {
		      if {[WPCmd PEFolder isincoming $colid] == 0 && [llength $flist]} {
			cgi_image_button delquery=[WPimg but_folddel] border=0 alt=Delete
			cgi_image_button renquery=[WPimg but_foldren] border=0 alt=Rename style=margin-left:4
			cgi_image_button dwnquery=[WPimg but_foldexp] border=0 alt=Export style=margin-left:4
		      }
		    }

		    if {[llength $collections] > 1} {
		      if {[set index [lsearch $shown $colid]] < 0} {
			set colpref [cgi_url [cgi_imglink expand] "${baseurl}expand=$colid" name=f_$colid]
		      } else {
			set colpref [cgi_url [cgi_imglink contract] "${baseurl}contract=$colid" name=f_$colid]
		      }
		    } else {
		      set colpref ""
		    }

		    cgi_table_data align=left "style=\"padding-left:10\"" nowrap {
		      if {[llength [lindex $collectionfolders $i]]} {
			set flist [lindex $collectionfolders $i]
		      }

		      cgi_puts ${colpref}[cgi_span "style=font-family:Helvetica; size: large; padding-left:10" "[lindex $col 1]"]
		    }

		    if {[llength $flist]} {
		      if {[WPCmd PEFolder isincoming $colid] == 0} {
			cgi_table_data valign=middle nowrap {
			  cgi_submit_button "new_$colid=Create New..."
			  cgi_submit_button "imp_$colid=Import..."
			}
		      }
		    }
		  }

		  if {[llength $flist]} {
		    blat_folder_list $colid $flist $shown $colid $baseurl $scroll anchorcnt 1
		  }

		  cgi_table_row {
		    cgi_table_data height=12 {
		      cgi_put [cgi_nbspace]
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

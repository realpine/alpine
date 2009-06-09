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
  {uid		{}	0}
  {show		{}	""}
  {expand	{}	""}
  {contract	{}	""}
  {target	{}	""}
  {onselect	{}	"compose"}
  {oncancel	{}	"index"}
  {controls	{}	1}
  {reload}
}

#  Output:
#
#	HTML/Javascript/CSS data representing the message specified
#	by the 'uid' argument

# the name of this script
set me [file tail [info script]]

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


set key [WPCmd PEInfo key]


#
# Display the given folder list in a table (w/ mihodge mods)
#
proc blat_folder_list {colid flist shown path baseurl scroll anchorcntref onselect} {
  global key controls target

  upvar $anchorcntref anchorcnt
  cgi_table width="100%" border=0 cellspacing=0 cellpadding=0 {
    cgi_table_row {
      cgi_table_data width=12 {
	cgi_puts [cgi_nbspace]
      }
    }

    set delim [WPCmd PEFolder delimiter $colid]
    set pl [llength $path]

    foreach folder $flist {
      set t [lindex $folder 0]
      set f [lindex $folder 1]
      set ff [linsert $path [llength $path] $f]
      set index -1;

      cgi_table_row {
	cgi_table_data width=12 {
	  cgi_puts [cgi_nbspace]
	}

	cgi_table_data width=18 valign=top {
	  if {[string first $t D] >= 0} {
	    if {[set index [lsearch $shown $ff]] < 0} {
	      cgi_puts [cgi_url [cgi_imglink expand] "${baseurl}expand=[WPPercentQuote $ff]"]
	    } else {
	      cgi_puts [cgi_url [cgi_imglink contract] "${baseurl}contract=[WPPercentQuote $ff]"]
	    }
	  } else {
	    cgi_puts [cgi_nbspace]
	  }
	}
	
	cgi_table_data {
	    if {[string first $t D] >= 0} {
	      cgi_puts "[cgi_anchor_name f_[WPPercentQuote $ff]]$f"
	      if {$index >= 0} {
		cgi_br
		set nflist [eval WPCmd PEFolder list $ff]
		blat_folder_list $colid $nflist $shown [lappend path $f] $baseurl $scroll anchorcnt $onselect
	      }
	    } else {
	      cgi_put [cgi_nbspace]

	      regsub -all {(')} [lrange $ff 1 end] {\\\\\1} ef
	      if {[string length $target] == 0} {
		set target "_top"
	      }

	      cgi_put [cgi_url $f wp.tcl?page=[join ${onselect} {&}]&f_colid=${colid}&f_name=[WPPercentQuote [join $ef $delim]]&target=${target}&cid=$key target=${target}]
	    }

	    incr anchorcnt
	  }
	}
      }
      
      cgi_table_row {
	cgi_table_data width=12 {
	  cgi_puts [cgi_nbspace]
	}
      }
    }
}

#
# Command Menu definition for Message View Screen
#
set folder_menu {
}

set common_menu {
  {
    {expr {$controls == 1}}
    {
      {
	# * * * * CANCEL * * * *
	cgi_puts [cgi_url Cancel wp.tcl?page=$oncancel&cid=[WPCmd PEInfo key] class=navbar target=_top]
      }
    }
  }
  {}
  {
    {}
    {
      {
	cgi_puts "Get Help"
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

if {[catch {WPNewMail $reload} newmail]} {
  error [list _action "new mail" $newmail]
}


# perform any requested actions

# preserve vars that my have been overridden with cgi parms

if {[catch {WPCmd PEFolder collections} collections]} {
  error [list _action "Collectoin list" $collections]
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

set baseurl wp.tcl?page=fldrbrowse&onselect=[WPPercentQuote ${onselect}]&oncancel=${oncancel}&controls=${controls}&target=${target}&

if {[llength $shown]} {
  append baseurl "show=[WPPercentQuote [join $shown ,]]&"
}


# paint the page
cgi_http_head {
  WPStdHttpHdrs text/html
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "Folder List" folders
    if {$controls == 1} {
      WPHtmlHdrReload "$_wp(appdir)/wp.tcl?page=folders"
    }

    WPStyleSheets
  }

  cgi_body bgcolor=$_wp(bordercolor) {

    catch {WPCmd PEInfo set help_context folders}

    # prepare context and navigation information

    set navops ""

    if {$controls == 1} {
      WPTFTitle "Browse Folder" $newmail
    }

    cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {
      cgi_table_row {
	if {$controls > 0} {
	  cgi_table_data rowspan=2 valign=top class=navbar {
	    cgi_table bgcolor=$_wp(menucolor) border=0 cellspacing=0 cellpadding=0 {
	      # next comes the menu down the left side, with suitable
	      cgi_table_row {
		eval {
		  cgi_table_data $_wp(menuargs) class=navbar {
		    WPTFCommandMenu folder_menu common_menu
		  }
		}
	      }
	    }
	  }
	}

	# down the right side of the table is the window's contents
	cgi_table_data width="100%" align=center valign=top class=dialog {

	  # then the table representing the folders
	  cgi_table width=75% border=0 cellspacing=0 cellpadding=2 align=center {

	    cgi_table_row {
	      cgi_table_data height=80 align=center valign=middle class=dialog {
		switch $controls {
		  0 -
		  2 { set task "the Saved message" }
		  default { set task "your composition's [cgi_italic "File Carbon Copy"] (Fcc)" }
		}

		cgi_puts "Click a folder name below to use it as the destination for $task, or click [cgi_italic Cancel]"
		cgi_puts "to return without choosing anything."
	      }
	    }

	    if {$controls != 1} {
	      cgi_table_row {
		cgi_table_data align=center valign=top height=40 class=navbar {
		  cgi_form $_wp(appdir)/wp method=get target=$target {
		    cgi_text page=$oncancel type=hidden notab
		    cgi_text cid=[WPCmd PEInfo key] type=hidden notab
		    cgi_submit_button cancel=Cancel
		  }
		}
	      }
	    }

	    cgi_table_row {
	      cgi_table_data {
		cgi_table border=0 cellspacing=0 cellpadding=2 align=center {
		  for {set i 0} {$i < [llength $collections]} {incr i} {
		    set col [lindex $collections $i]
		    set colid [lindex $col 0]

		    cgi_table_row {
		      cgi_table_data width=18 valign=top {
			if {[llength $collections] > 1} {
			  if {[set index [lsearch $shown $colid]] < 0} {
			    cgi_puts [cgi_url [cgi_imglink expand] "${baseurl}expand=$colid"]
			  } else {
			    cgi_puts [cgi_url [cgi_imglink contract] "${baseurl}contract=$colid"]
			  }
			} else {
			  cgi_puts [cgi_nbspace]
			}
		      }
		      
		      cgi_table_data align=left {
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

			if {[llength $flist]} {
			  set flist [WPCmd PEFolder list $colid]
			}

			if {$scroll == $colid} {
			  #cgi_javascript {cgi_puts {scroll = window.document.anchors.length;}}
			}

			cgi_puts [cgi_font face=Helvetica size=+1 "[lindex $col 1]<A NAME=\"f_$colid\">&nbsp;</A>"]

			incr anchorcnt

			if {[llength $flist]} {
			  blat_folder_list $colid $flist $shown $colid $baseurl $scroll anchorcnt $onselect
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
}

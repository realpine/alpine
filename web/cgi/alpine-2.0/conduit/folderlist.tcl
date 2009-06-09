#!./tclsh
# $Id: folderlist.tcl 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

#  folderlist.tcl
#
#  Purpose:  CGI script that generates a page snippet that displays
#            the XHR requested folder list
#
#  Input:    PATH_INFO: [/<col_number>]/<directory_path>
#            along with possible search parameters:
set folderlist_args {
  {op	{}	"noop"}
}

# inherit global config
source ../alpine.tcl
source ../foldercache.tcl
source ../common.tcl


# default folderlist.tcl state
set c 0

# TEST
proc cgi_suffix {args} {
  return ""
}

if {[info exists env(PATH_INFO)] && [string length $env(PATH_INFO)]} {
  if {0 == [string compare $env(PATH_INFO) "/"] || 0 == [string compare $env(PATH_INFO) "//"]} {
    set c -1
    set dir ""
  }

  if {$c < 0 || [regexp {^/([0-9]+)(/([^/]*/)*)$} $env(PATH_INFO) dummy c dir]} {
    # Import data validate it and get session id
    if {[catch {WPGetInputAndID sessid} result]} {
      set harderr "Cannot init session: $result"
    } else {
      # grok parameters
      foreach item $folderlist_args {
	if {[catch {eval WPImport $item} result]} {
	  set harderr "Cannot read input: $result"
	  break
	}
      }
    }
  } else {
    set harderr "BOTCH: Invalid Request: $env(PATH_INFO)"
  }
} else {
  set harderr "BOTCH: No Folder Specified"
}
cgi_puts "Content-type: text/html; charset=\"UTF-8\"\n"

if {[info exists harderr]} {
  cgi_puts "<b>ERROR: $harderr</b>"
  exit
}

set cs [WPCmd PEFolder collections]

if {$c < 0} {
  cgi_division class="flistContext" {
    cgi_put "[cgi_span "class=sp spfcl spfcl2" [cgi_span "style=display:none;" "Folders: "]][cgi_span "Folder Collections"]"
    set folderpath ""
  }

  cgi_division class=flistFolders {
    cgi_table cellspacing="0" cellpadding="0" class="flt" {
      foreach col $cs {
	set ci [lindex $col 0]
	set onclick "onClick=return newFolderList(this.parentNode.parentNode.parentNode.parentNode.parentNode.parentNode,null,$ci);"
	cgi_table_row {
	  cgi_table_data class="fli" {
	    cgi_puts [cgi_url [cgi_span "class=sp spfl spfl1" [cgi_span "style=display:none;" [cgi_gt]]] "list/${ci}/" title="View folders in collection." $onclick]
	  }
	  cgi_table_data class="fln" {
	    cgi_puts "[lindex $col 1]"
	  }
	}
	cgi_table_row {
	  cgi_table_data colspan=2 class="flcd" {
	    cgi_puts "[cgi_span class=flcd [lindex $col 2]]"
	  }
	}
      }
    }
  }
  cgi_puts "<script>"
  cgi_puts "updateElementValue('pickFolderCollection', '-1');"
  cgi_puts "updateElementValue('pickFolderPath', '/');"
  wpStatusAndNewmailJavascript
  cgi_puts "setPanelBodyWidth('flistFolders');"
  cgi_puts "</script>"
} else {
  set delim [WPCmd PEFolder delimiter $c]

  switch -- $op {
    delete {
      if {0 == [catch {cgi_import_as f delf}]} {
	if {[string compare -nocase inbox $delf]} {
	  if {[catch [concat WPCmd PEFolder delete $c [regsub -all $delim $dir { }] $delf] result]} {
	    WPCmd PEInfo statmsg "Cannot delete $result"
	  } else {
	    WPCmd PEInfo statmsg "$delf permanently removed"
	    removeFolderCache $c [join [list $dir $delf] $delim]
	  }
	} else {
	  WPCmd PEInfo statmsg "Cannot delete INBOX"
	}
      } else {
	WPCmd PEInfo statmsg "Delete which folder???"
      }
    }
    add {
      if {0 == [catch {cgi_import_as f addf}]} {
	if {[string compare -nocase inbox $addf]} {
	  if {[catch [concat WPCmd PEFolder create $c [regsub -all $delim $dir { }] $addf] result]} {
	    WPCmd PEInfo statmsg "Folder Cannot add: $result"
	  } else {
	    WPCmd PEInfo statmsg "Folder $addf added"
	  }
	} else {
	  WPCmd PEInfo statmsg "Cannot add INBOX"
	}
      } else {
	WPCmd PEInfo statmsg "No foldername provided"
      }
    }
    rename {
      if {0 == [catch {cgi_import sf}] && 0 == [catch {cgi_import df}]} {
	if {[string compare -nocase inbox $sf] && [string compare -nocase inbox $df]} {
	  if {[catch {WPCmd PEFolder rename $c [join [list $dir $sf] $delim] [join [list $dir $df] $delim]} result]} {
	    WPCmd PEInfo statmsg "Cannot rename: $result"
	  } else {
	    WPCmd PEInfo statmsg "Renamed $sf to $df"
	  }
	} else {
	  WPCmd PEInfo statmsg "Cannot rename INBOX"
	}
      } else {
	WPCmd PEInfo statmsg "Foldernames not provided"
      }
    }
    noop {
    }
    default {
      WPCmd PEInfo statmsg "Unrecognized option: $op"
    }
  }

  if {0 == [string compare $dir {/}]} {
    if {[catch {WPCmd PEFolder list $c} flist]} {
      set authlist [wpHandleAuthException $flist [list [lindex [lindex $cs $c] 0] "list folders in collection [lindex [lindex $cs $c] 1]"] INBOX]
      if {0 == [llength $authlist]} {
	WPCmd PEInfo statmsg $flist
      }
    }
  } else {
    if {[catch {WPCmd PEFolder list $c $dir} flist]} {
      set col [lindex [lindex $cs $c] 1]

      set authlist [wpHandleAuthException $flist [list [lindex [lindex $cs $c] 0] "list folders in collection [lindex [lindex $cs $c] 1]"] INBOX]
      if {0 == [llength $authlist]} {
	WPCmd PEInfo statmsg $flist
      }
    }
  }

  cgi_division class="flistContext" {

    if {[llength $cs] > 1} {
      cgi_put "[cgi_span "class=sp spfcl spfcl2" "style=border-bottom: 1px solid #003399;" "onClick=\"newFolderList(this.parentNode.parentNode,null,'');\"" [cgi_span "style=display:none;" "Folders: "]]"
    } else {
      cgi_put [cgi_span "class=sp spfcl spfcl2" [cgi_span "style=display:none;" "Folders: "]]
    }

    set dp ""
    set col [lindex [lindex $cs $c] 1]

    if {[regexp {^/(.*)/$} $dir dummy folderpath]} {
      cgi_puts [cgi_url $col "list/$c/" title="List folders in this directory" "onClick=return newFolderList(this.parentNode.parentNode,null,$c);"]
      set ds [split $folderpath {/}]
      for {set i 0} {$i < [llength $ds]} {incr i} {
	set d [lindex $ds $i]
	if {[string length $d]} {
	  lappend dp $d
	  set path [join $dp {/}]
	  if {$i < ([llength $ds] - 1)} {
	    set d [cgi_url "${d}" "list/$c/$path/" title="List subfolders" "onClick=return newFolderList(this.parentNode.parentNode,null,$c,'$path');"]
	  } else {
	    set d [span class=flistFolder $d]
	  }

	  cgi_puts "[cgi_span "&raquo;"]$d"
	}
      }
    } else {
      set folderpath ""
      cgi_puts [cgi_span $col]
    }
  }

  cgi_division class=flistFolders id=flistFolders {
    cgi_table cellspacing="0" cellpadding="0" class="flt" {
      foreach f $flist {
	set t [lindex $f 0]
	set fn [lindex $f 1]
	set onclick "onClick=return newFolderList(this.parentNode.parentNode.parentNode.parentNode.parentNode.parentNode,null,${c},'[file join ${folderpath} ${fn}]');"
	cgi_table_row class="flr" {
	  cgi_table_data class="fli" {
	    if {[string first D $t] >= 0} {
	      cgi_puts [cgi_url [cgi_span "class=sp spfl spfl1" [cgi_span "style=display:none;" [cgi_gt]]] "${fn}/" title="List subfolders" $onclick]
	    } else {
	      cgi_puts [cgi_nbspace]
	    }
	  }
	  cgi_table_data class="fln" {
	    set hfn [cgi_quote_html $fn]
	    if {[string first F $t] >= 0} {
	      cgi_puts [cgi_url "$hfn" "browse/${c}[WPPercentQuote "${dir}${fn}" {/}]" class="fln" title="Click to select." "onClick=return flistPick(this,'[WPPercentQuote $fn]');" "onDblClick=return flistPickPick(this);"]
	    } else {
	      cgi_puts "$hfn"
	    }
	  }
	}
      }
    }
  }
  cgi_puts "<script>"
  if {$c >= 0} {
    cgi_puts "YAHOO.alpine.current.incoming = [WPCmd PEFolder isincoming $c];"
  }
  cgi_puts "updateElementValue('pickFolderCollection', '$c');"
  cgi_puts "updateElementValue('pickFolderPath', '$folderpath');"
  wpStatusAndNewmailJavascript
  cgi_puts "setPanelBodyWidth('flistFolders');"
  if {[info exists authlist]} {
    reportAuthException $authlist
  }
  cgi_puts "</script>"
}

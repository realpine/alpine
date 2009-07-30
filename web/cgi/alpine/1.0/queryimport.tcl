#!./tclsh
# $Id: queryimport.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  queryimport.tcl
#
#  Purpose:  CGI script to generate html form used to ask for 
#            importing a folder

#  Input:
set import_vars {
  {fid	"No Collection Specified"}
}

#  Output:
#
#	HTML/CSS data representing the form

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

set query_menu {
  {
    {expr {0}}
    {
      {
	cgi_puts "Get Help"
      }
    }
  }
}

WPEval $import_vars {

  set colid [lindex $fid 0]
  if {[llength $fid] > 1} {
    set fpath [eval "file join [lrange $fid 1 end]"]
  } else {
    set fpath ""
  }

  if {[catch {WPCmd PEFolder collections} collections]} {
    catch {WPCmd PEInfo statmsg "Can't Import: $collections"}
    cgi_http_head {
      cgi_redirect [cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders.tcl
    }
  } elseif {$colid < 0 || $colid > [llength $collections]} {
    catch {WPCmd PEInfo statmsg "Can't Import: Invalid collection: $colid"}
    cgi_http_head {
      cgi_redirect [cgi_root]/$_wp(appdir)/$_wp(ui1dir)/wp.tcl?page=folders.tcl
    }
  } else {

    if {[string length $fpath]} {
      set coldesc "the directory [cgi_bold $fpath] within "
    }

    append coldesc "the collection [cgi_bold [lindex [lindex $collections $colid] 1]]"

    cgi_http_head {
      WPStdHttpHdrs
    }

    cgi_html {
      cgi_head {
	WPStdHtmlHdr "Import"
	WPStyleSheets
	cgi_put  "<style type='text/css'>"
	cgi_put  ".filename	{ font-family: Courier, monospace ; font-size: 10pt }"
	cgi_puts "</style>"
      }

      cgi_body BGCOLOR="$_wp(bordercolor)" {

	cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post enctype=multipart/form-data target=_top {
	  cgi_text page=folders type=hidden notab
	  cgi_text cid=[WPCmd PEInfo key] type=hidden notab
	  cgi_text fid=$fid type=hidden notab

	  cgi_table border=0 cellpadding=0 cellspacing=0 width="100%" height="100%" {
	    cgi_table_row {
	      eval {
		cgi_table_data $_wp(menuargs) {
		  WPTFCommandMenu query_menu {}
		}
	      }

	      cgi_table_data align=center valign=top class=dialog {
		cgi_table border=0 width=75% cellpadding=15 {
		  cgi_table_row {
		    cgi_table_data align=center "style=\"padding-top:30\"" {
		      cgi_puts "Folder Import copies a mail folder, typically created by the Export command, from the computer your browser is running on into a new Web Alpine folder.  [cgi_nbspace]Successful Import consists of three steps."
		      cgi_p
		      cgi_puts "First, enter the path and filename of the folder below.  Use the [cgi_italic Browse] button to help choose the file."
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_file_button file "accept=*/*" size=30 class=filename
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_puts "Second, provide a [cgi_bold unique] name for the imported folder to be assigned within $coldesc:"
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_text iname= maxlength=256 size=40 class=filename
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_puts "Finally, click [cgi_italic "Import File"] to copy the folder into WebPine, or [cgi_italic Cancel] to return to the folder list."
		    }
		  }

		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_submit_button "import=Import File" class=navtext
		      cgi_submit_button cancel=Cancel class=navtext
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

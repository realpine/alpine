#!./tclsh
# $Id: fr_filtedit.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  fr_conf_process.tcl
#
#  Purpose:  CGI script to generate frame set for config operations
#	     in webpine-lite pages.
#            This page assumes that it was loaded by conf_process.tcl

#  Input:
set frame_vars {
}

#  Output:
#

# inherit global config
source ./alpine.tcl
source cmdfunc.tcl
source filter.tcl

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {
    cgi_head {
      WPStdHtmlHdr "Filter Configuration"
    }

    cgi_frameset "rows=$_wp(titleheight),*" resize=yes border=0 frameborder=0 framespacing=0 {
      if {[info exists filtedit_add] && 1 == $filtedit_add} {
	set title 153
      } else {
	set title 154
      }

      set parms ""
      if {[info exists filtedit_add]} {
	set parms "${parms}&add=${filtedit_add}"
      }
      if {[info exists filtedit_fno]} {
	set parms "${parms}&fno=${filtedit_fno}"
      }
      if {[info exists filtedit_onfiltcancel]} {
	set parms "${parms}&onfiltcancel=${filtedit_onfiltcancel}"
      }
      if {[info exists filtedit_indexcolor]} {
	set parms "${parms}&filtedit_indexcolor=1"
	if {[info exists fg] && [string length $fg]} {
	  set parms "${parms}&fg=$fg"
	}
	if {[info exists bg] && [string length $bg]} {
	  set parms "${parms}&bg=$bg"
	}
	if {[info exists fgorbg]} {
	  set parms "${parms}&fgorbg=$fgorbg"
	}
      } elseif {[info exists filtedit_score]} {
	set parms "${parms}&filtedit_score=1"
	if {[info exists scoreval] && [string length $scoreval]} {
	  set parms "${parms}&scoreval=$scoreval"
	}
	if {[info exists scorehdr] && [string length $scorehdr]} {
	  set parms "${parms}&scoreval=$scorehdr"
	}
      }

      if {[info exists filterrtext]} {
	# relay pattern elements
	set parms "${parms}&filterrtext=${filterrtext}"
	foreach {pvar pexp} $pattern_id {
	  if {[info exists $pvar]} {
	    if {[string length [set pval [subst $$pvar]]]} {
	      append parms "&${pvar}=${pval}"
	    }
	  }
	}

	foreach {pvar pexp} $pattern_fields {
	  if {[info exists $pvar]} {
	    if {[string length [set pval [subst $$pvar]]]} {
	      append parms "&${pvar}=${pval}"
	    }
	  }
	}

	# relay various pattern actions
	if {[info exists action] || 0 == [catch {WPImport action}]} {
	  append parms "&action=$action"
	}

	if {0 == [catch {WPImport actionfolder}]} {
	  append parms "&actionfolder=$actionfolder"
	}

	if {0 == [catch {WPImport moind}] && [string length $moind]} {
	  append parms "&moind=$moind"
	}

	if {0 == [catch {WPImport actionfolder}]} {
	  append parms "&actionfolder=$actionfolder"
	}

	if {[info exists folder] == 0} {
	  wpGetVarAs folder folder
	}

	if {[string length $folder]} {
	  append parms "&folder=$folder"
	}

	if {[info exists ftype] == 0} {
	  wpGetVarAs ftype ftype
	}

	if {[string length $ftype]} {
	  append parms "&ftype=$ftype"
	}
      }

      cgi_frame hdr=header.tcl?title=${title}
      cgi_frame body=wp.tcl?page=filtedit&cid=$cid&oncancel=$oncancel$parms
    }
  }


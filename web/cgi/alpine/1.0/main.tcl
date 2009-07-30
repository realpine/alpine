# $Id: main.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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


#  Input:

#  Output:
#

proc save_hack {} {
  if {[catch {WPImport f_name "x"}] == 0 && [catch {WPImport f_colid "x"}] == 0} {
    append parms "&f_name=${f_name}&f_colid=${f_colid}"

    if {[catch {WPImport send "x"}] == 0} {
      append parms "&send=${send}"
    }

    return $parms
  }

  error "not saving"
}

cgi_http_head {
  WPStdHttpHdrs
  WPExportCookie sessid "$_wp(sessid)@[info hostname]" $_wp(appdir)/$_wp(ui1dir)
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "WebPine"
  }

  cgi_frameset "cols=112,*" frameborder=0 framespacing=0 {
    cgi_frame gen=common.tcl?m=[WPCmd PEMailbox mailboxname]&c=[WPCmd PEInfo key]&v=[WPScriptVersion common]&q=[WPCmd PEInfo feature quit-without-confirm] title="Navigation Commands"

    if {[catch {WPCmd PEInfo set wp_spec_script} script]} {
      set script fr_index.tcl
    }

    set parms ""

    if {[info exists frame_vars]} {
      foreach v $frame_vars {
	if {[string length [subst $[lindex $v 0]]]} {
	  append parms "&[lindex $v 0]=[subst $[lindex $v 0]]"
	}
      }
    }

    switch -regexp $script {
      ^fr_view.tcl$ {
	if {[catch {save_hack} x] == 0} {
	  append parms "&$x"
	}
	  
	if {[catch {WPCmd PEInfo set uid} uid] == 0} {
	  append parms "&uid=$uid"
	}

	if {[catch {WPCmd PEInfo set op} op] == 0} {
	  append parms "&op=$op"
	}
      }
      ^fr_index.tcl$ -
      ^fr_main.tcl$ {
	if {[catch {save_hack} x] == 0} {
	  append parms "&$x"
	}
      }
    }

    cgi_frame spec=${script}?c=[WPCmd PEInfo key]${parms} frameborder=0 title="Message List and View"
  }
}

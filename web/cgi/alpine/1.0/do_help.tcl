# $Id: do_help.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

cgi_http_head {
  WPStdHttpHdrs
}

cgi_html {
  if {![string length $topic]} {
    if {[catch {WPCmd PEInfo set help_context} s] == 0} {
      set topic $s
      catch {WPCmd PEInfo unset help_context}
    }
  }

  cgi_head {
    cgi_title "Alpine Help"
  }

  cgi_frameset "cols=112,*" frameborder=0 framespacing=0 border=0 {
    set parms ""

    foreach v $help_vars {
      set val [subst $[lindex $v 0]]
      if {[string length $val]} {
	if {[string length $parms]} {
	  append parms "&"
	} else {
	  append parms "?"
	}

	append parms "[lindex $v 0]=$val"
      }
    }

    cgi_frame bodindx=helpindex.tcl$parms title="Help Navigation"
    cgi_frame bodtext=helpbody.tcl$parms title="Help Text"
  }
}

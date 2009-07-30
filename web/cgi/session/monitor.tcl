#!./tclsh
# $Id: monitor.tcl 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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

#  monitor.tcl

# read config
source ./alpine.tcl

proc nicetime {timeoutput} {
  if {[regexp {^[0-9]+ } $timeoutput msec]} {
    return "[format {%d.%06d} [expr {$msec / 1000000}] [expr {$msec % 1000000}]] seconds"
  } else {
    return $timeoutput
  }
}

# take process snapshot
#set cmd "/bin/ps -lC alpined --sort=cutime"
set cmd "/bin/ps -auxww --sort=cutime"
if {[catch "exec $cmd" result]} {
  set prohdr "ps error: $result"
  set proclist {}
} else {
  set r [split $result "\n"]
  set prochdr [lindex $r 0]
  set proclist [lrange $r 1 end]
}

cgi_eval {
  cgi_html {
    cgi_head {
      cgi_title "Web Alpine Monitor"
      cgi_puts  "<style type='text/css'>"
      cgi_puts  ".monsec { text-decoration: underline ; margin: 4}"
      cgi_puts "</style>"
    }

    cgi_body {
      cgi_h2 "WebPine Status // [info hostname] // [clock format [clock seconds]]"

      ##
      ## system performance monitor
      ##n
      cgi_preformatted {
	# simple server load
	set cmd "/usr/ucb/uptime"
	if {[catch "exec $cmd" result]} {
	  cgi_puts "uptime unavailable: $result"
	} else {
	  cgi_puts [cgi_span class=monsec "Server uptime"]
	  foreach l [split $result "\n"] {
	    cgi_puts "  $l"
	  }
	}

	cgi_br

	# list alpined adapters
	foreach l $proclist {
	  if {[regexp $_wp(servlet) $l] || [regexp $_wp(pc_servlet) $l]} {
	    lappend adapters $l
	  }
	}

	cgi_puts [cgi_span class=monsec "WebPine Adapters ([llength $adapters])"]
	cgi_puts "  $prochdr"
	foreach l $adapters {
	  cgi_puts "  $l"
	}

	cgi_br

	# tmp disc usage
	cgi_puts [cgi_span class=monsec "Temp Directory Usage ($_wp(tmpdir))"]
	set cmd "/bin/df $_wp(tmpdir)"
	if {[catch "exec $cmd" result]} {
	  cgi_puts "usage unavailable: $result"
	} else {
	  foreach l [split $result "\n"] {
	    cgi_puts "  $l"
	  }
	}

	cgi_br 

	# detach staging usage
	cgi_puts [cgi_span class=monsec "Detach Staging Usage ($_wp(tmpdir))"]
	set cmd "/bin/df $_wp(detachpath)"
	if {[catch "exec $cmd" result]} {
	  cgi_puts "usage unavailable: $result"
	} else {
	  foreach l [split $result "\n"] {
	    cgi_puts "  $l"
	  }
	}

	if {[info exists report_env]} {
	  cgi_br

	  cgi_puts [cgi_span class=monsec "Environment:"]

	  set cgiv {
	    SERVER_SOFTWARE
	    SERVER_NAME
	    GATEWAY_INTERFACE
	    SERVER_PROTOCOL
	    SERVER_PORT
	    REQUEST_METHOD
	    PATH_INFO
	    PATH_TRANSLATED
	    SCRIPT_NAME
	    QUERY_STRING
	    REMOTE_HOST
	    REMOTE_ADDR
	    AUTH_TYPE
	    REMOTE_USER
	    REMOTE_IDENT
	    CONTENT_TYPE
	    CONTENT_LENGTH
	    HTTP_ACCEPT
	    HTTP_USER_AGENT
	  }
	  foreach v $cgiv {
	    if {[info exists env($v)]} {
	      cgi_puts "  $v: $env($v)"
	    }
	  }
	}	


	##
	## session specific feedback
	##
	if {[info exists _wp(monitors)]
	    && [info exists env(REMOTE_USER)]
	    && [lsearch -exact $_wp(monitors) $env(REMOTE_USER)] >= 0} {

	  cgi_br

	  cgi_puts [cgi_span class=monsec "Kerberos ticket cache info"]
	  foreach l [glob "[file join $_wp(tmpdir) krb]*"] {
	    set file [file join $_wp(tmpdir) $l]
	    cgi_put "  [exec /bin/ls -l $file]"
	    if {[catch {expr {[clock seconds] - [file mtime $file]}} d]} {
	    } else {
	      cgi_puts "  ([expr {$d / 3600}] hours, [expr {($d % 3600) / 60}] minutes old)"
	    }
	  }

	  cgi_br

	  cgi_puts [cgi_span class=monsec "uid_mapper Process"]
	  # Condition of uid_mapper
	  cgi_puts "  $prochdr"
	  foreach l $proclist {
	    if {[regexp uidmapper $l]} {
	      lappend umlist $l
	    }
	  }

	  if {[info exists umlist]} {
	    foreach l $umlist {
	      cgi_puts "  $l"
	    }
	  } else {
	    cgi_puts "  HELP!!! NO UIDMAPPER RUNNING!!!"
	  }

	  cgi_br

	  if {[info exists _wp(hosts)] && [llength $_wp(hosts)]} {
	    cgi_puts [cgi_span class=monsec "Session Performance (netid: $env(REMOTE_USER))"]

	    set sdata [lindex $_wp(hosts) 0]
	    set User $env(REMOTE_USER)
	    set env(IMAP_SERVER) "[subst [lindex $sdata 1]]/user=$env(REMOTE_USER)"

	    if {[llength $sdata] > 2 && [string length [lindex $sdata 2]]} {
	      set defconf [subst [lindex $sdata 2]]
	      set confloc "\{$env(IMAP_SERVER)\}$_wp(config)"
	      cgi_puts "  User Config: $confloc"

	      # launch session
	      cgi_put "  alpined Launch: "
	      set ct [time {
		if {[catch {exec [file join $_wp(bin) launch.tcl]} _wp(sessid)]} {
		  set err "FAILURE: $_wp(sessid)"
		} else {
		  WPValidId $_wp(sessid)
		}
	      }]

	      if {[info exists err]} {
		cgi_puts $err
	      } else {
		cgi_puts [nicetime $ct]

		cgi_put "  Open Inbox: "
		set ct [time {
		  if {[catch {WPCmd PESession open $env(REMOTE_USER) "" $confloc $defconf} answer]} {
		    set err "FAILURE: "
		    if {[info exists answer]} {
		      if {[string length $answer] == 0} {
			append err "Unknown Username or Incorrect Password"
		      } else {
			append err $answer
		      }
		    } else {
		      append err "Unknown reason"
		    }
		  }
		}]

		if {[info exists err]} {
		  cgi_puts $err
		} else {
		  cgi_puts [nicetime $ct]

		  cgi_put "  Fetch First Message: "

		  set ct [time {
		    if {[catch {
			         set msg [WPCmd PEMailbox first]
			         set uid [WPCmd PEMailbox uid $msg]
			         set txt [WPCmd PEMessage $uid text]
			       } txt]} {
		      set err $txt
		    }
		  }]

		  if {[info exists err]} {
		    cgi_puts "FAILURE: $err"
		  } else {
		    cgi_puts [nicetime $ct]

		    cgi_put "  Fetch Last Message: "

		    set ct [time {
		      if {[catch {
				    set msg [WPCmd PEMailbox last]
				    set uid [WPCmd PEMailbox uid $msg]
				    set txt [WPCmd PEMessage $uid text]
				  } txt]} {
			set err $txt
		      }
		    }]

		    if {[info exists err]} {
		      cgi_puts "FAILURE: $err"
		    } else {
		      cgi_puts [nicetime $ct]
		    }
		  }
		}

		set ct [time {
		  catch {WPCmd PESession close}
		  catch {WPCmd exit}
		}]

		cgi_puts "  Close Session: [nicetime $ct]"
	      }
	    } else {
	      cgi_puts "Invalid host configuration"
	    }

	  }
	}
      }
    }
  }
}
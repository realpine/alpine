#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl

    cgi_input

    cgi_title "Kill runaway CGI processes"

    cgi_body {
	if {0==[catch {cgi_import PidList}]} {
	    catch {cgi_import Refresh;set PidList {}}
	    cgi_import Sig
	    h4 "If this were not a demo, the following commands would be executed:"
	    foreach pid $PidList {
		# to undemoize this and allow processes to be killed,
		# change h5 to exec and remove the quotes
		if {[catch {h5 "kill -$Sig $pid"} msg]} {
		    h4 "$msg"
		}
	    }
	}

	cgi_form kill {
	    set ps /bin/ps
	    if {[file exists /usr/ucb/ps]} {
		set ps /usr/ucb/ps
	    }

	    set f [open "|$ps -auxww" r]
	    table border=2 {
		table_row {
		    table_data {puts kill}
		    table_data {cgi_preformatted {puts [gets $f]}}
		}
		while {-1 != [gets $f buf]} {
		    if {[regexp "$argv0" $buf]} continue
		    if {![regexp "^http" $buf]} continue
		    table_row {
			table_data {
			    scan $buf "%*s %d" pid
			    cgi_checkbox PidList=$pid
			}
			table_data {
			    cgi_preformatted {puts $buf}
			}
		    }
		}
		
	    }

	    submit_button "=Send signal to selected processes"
	    submit_button "Refresh=Refresh listing"
	    reset_button

	    br; radio_button "Sig=TERM" checked; puts "SIGTERM: terminate gracefully"
	    br; radio_button "Sig=KILL";	 puts "SIGKILL: terminate ungracefully"
	    br; radio_button "Sig=STOP";	 puts "SIGSTOP: suspend"
	    br; radio_button "Sig=CONT";	 puts "SIGCONT: continue"

	    p "SIGSTOP and SIGCONT are particularly useful if the
	    processes aren't yours and the owner isn't around to ask.
	    Suspend them and let the owner decide later whether to
	    continue or kill them."
	}
    }
}

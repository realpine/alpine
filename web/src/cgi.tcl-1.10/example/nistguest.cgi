#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl
    cgi_input

    cgi_title "NIST Guest Book"

    cgi_uid_check http

    set BarURL $EXPECT_ART/pink.gif

    set Q(filename) "$DATADIR/nistguest"

    set statesNeeded {}

    proc poll_read {} {
	global Q

	set fid [open $Q(filename) r]
	while {-1!=[gets $fid StateFullName]} {
	    regsub " " $StateFullName "" State
	    set Q($State) $StateFullName
	    gets $fid buf
	    foreach "Q($State,1) Q($State,2) Q($State,3)" $buf {}
	    lappend Q(statesAll) $State
	    if {0 == [string compare "" "$Q($State,3)"]} {
		lappend Q(statesNeeded) $State
	    }
	}
	close $fid
    }

    proc poll_write {} {
	global Q

	# No file locking or real database handy so we can't guarantee that
	# simultaneous votes aren't dropped, but we'll at least avoid
	# corruption of the file by working on a private copy.
	set tmpfile $Q(filename).[pid]
	set fid [open $tmpfile w]
	foreach state $Q(statesAll) {
	    set data [list $Q($state,1) $Q($state,2) $Q($state,3)]
	    puts $fid $Q($state)\n$data
	}
	close $fid
	exec mv $tmpfile $Q(filename)
    }

    cgi_body {
	poll_read

	if {0 == [catch {cgi_import State}]} {
	    if {![info exists Q($State)]} {
		user_error "There is no such state: $State"
	    }		

	    set Name ""
	    catch {import Name}
	    if {0==[string length $Name]} {
		user_error "You didn't provide your name."
	    }

	    set Email ""
	    catch {import Email}

	    set Description ""
	    catch {import Description}
	    if {0==[string length $Description]} {
		user_error "You didn't provide a description."
	    }

	    set data "<Name $Name><Email $Email><Description $Description>"
	    # simplify poll_read by making each entry a single line
	    regsub -all \n $data " " data

	    if {0 == [string compare "" $Q($State,1)]} {
		set Q($State,1) $data
	    } elseif {0 == [string compare "" $Q($State,2)]} {
		set Q($State,2) $data
	    } else {
		set Q($State,3) $data
	    }

	    poll_write
	    puts "Thanks for your submission!"

	    return
	}

	form nistguest {
	    puts "In the spirit of Scriptics' request for Tcl success
stories, our group at NIST is looking for some too.  It's time for our
annual report to Congress.  So if your state appears in the list below
and you can provide a brief description of how our work has helped
you, we would appreciate hearing from you."
	    hr
	    br;puts "If your state does not appear in this list, then we
already have enough entries for your state.  Thanks anyway!"
	    br;puts "State:"
	    cgi_select State {
		foreach state $Q(statesNeeded) {
		    option "$Q($state)" value=$state
		}
	    }

	    br;puts "Full name:"
	    cgi_text Name=

	    br;puts "We probably won't need to contact you; But just in
case, please provide some means of doing so.  Email info will remain
confidential - you will NOT be put on any mailing lists."
	    br;puts "Email:"
	    cgi_text Email=
	    puts "(optional)"

	    p "Please describe a significant impact (e.g., goals
accomplished, hours/money saved, user expectations met or exceeded)
that NIST's Tcl-based work (Expect, cgi.tcl, APDE, APIB, EXPRESS
server, ...)  has had on your organization.  A brief paragraph is
fine."

	    cgi_textarea Description= rows=10 cols=80
	    br
	    submit_button "=Submit"
	}
    }
}

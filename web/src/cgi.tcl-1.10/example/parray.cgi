#!/depot/path/tclsh

# This is a CGI script that displays the environment or another array.

package require cgi

cgi_eval {
    source example.tcl

    proc arrays {} {
	uplevel #0 {
	    set buf {}
	    foreach a [info globals] {
		if {[array exists $a]} {
		    lappend buf $a
		}
	    }
	    return $buf
	}
    }

    cgi_input

    cgi_title "Display environment or another array"

    cgi_body {
	p "This script displays the environment or another array."
	if {[catch {cgi_import Name}]} {
	    set Name env
	}

	cgi_form parray {
	    cgi_select Name {
		foreach a [arrays] {
		    cgi_option $a selected_if_equal=$Name
		}
	    }
	    cgi_submit_button
	}

	global $Name
	if {[array exist $Name]} {
	    cgi_parray $Name
	} else {
	    puts "$Name: no such array"
	}
    }
}

#!/depot/path/tclsh

# This script implements the "Virtual Clock" used as the example in the
# paper describing CGI.pm, a perl module for generating CGI.
# Stein, L., "CGI.pm: A Perl Module for Creating Dynamic HTML Documents
# with CGI Scripts", SANS 96, May '96.

# Do you think it is more readable than the other version?
# (If you remove the comments and blank lines, it's exactly
# the same number of lines.)  See other comments after script. - Don

package require cgi

cgi_eval {
    source example.tcl

    cgi_input

    set format ""
    if {[llength [cgi_import_list]]} {
	if 0==[catch {cgi_import time}] {
	    append format [expr {[cgi_import type] == "12-hour"?"%r ":"%T "}]
	}
	catch {cgi_import day;          append format "%A "}
	catch {cgi_import month;        append format "%B "}
	catch {cgi_import day-of-month; append format "%d "}
	catch {cgi_import year;         append format "%Y "}
    } else {
	append format "%r %A %B %d %Y"
    }

    set time [clock format [clock seconds] -format $format]

    cgi_title "Virtual Clock"

    cgi_body {
	puts "At the tone, the time will be [strong $time]"
	hr
	h2 "Set Clock Format"

	cgi_form vclock {
	    puts "Show: "
	    foreach x {time day month day-of-month year} {
		cgi_checkbox $x checked
		put $x
	    }
	    br
	    puts "Time style: "
	    cgi_radio_button type=12-hour checked;put "12-hour"
	    cgi_radio_button type=24-hour        ;put "24-hour"
	    br
	    cgi_reset_button
	    cgi_submit_button =Set	    
	}
    }
}

# Notes:

# Time/date generation is built-in to Tcl.  Thus, no extra processes
# are necessary and the result is portable.  In contrast, CGI.pm
# only runs on UNIX.

# Displaying checkboxes side by side the way that CGI.pm does by
# default is awful.  The problem is that with enough buttons, it's not
# immediately clear if the button goes with the label on the right or
# left.  So cgi.tcl does not supply a proc to generate such a
# grouping.  I've followed CGI.pm's style here only to show that it's
# trivial to get the same affect, but the formatting in any real form
# is more wisely left to the user.

# Footer generation (<hr><address>...  at end of CGI.pm) is replaced
# by "source example.tcl".  Both take one line.

#!/depot/path/tclsh

# This CGI script displays the Tcl and Perl vclock source side by side.

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "Comparison of vclock source"

    cgi_frameset cols=50%,* {
	cgi_frame =[cgi_cgi display scriptname=vclock.cgi]
	cgi_frame =[cgi_cgi display scriptname=vclock.pl]
    }
    cgi_noframes {
	cgi_h1 "uh oh"

	p "This document is designed to be viewed by a Frames-capable
	browser.  If you see this message your browser is not
	Frames-capable."
    }
}

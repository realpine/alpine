#!/depot/path/tclsh

# This is a CGI script that displays the results of other CGI scripts
# in a separate frame.  It is only used for a few rare examples such
# as image.cgi

package require cgi

cgi_eval {
    source example.tcl

    cgi_input

    cgi_head {
	set scriptname image.cgi
	catch {cgi_import scriptname}
	set scriptname [file tail $scriptname]
	cgi_title $scriptname
    }
    cgi_frameset rows=50%,50% {
	cgi_frame =$scriptname?header=1
	cgi_frame =$scriptname
    }
    cgi_noframes {
	cgi_h1 "uh oh"

	p "This document is designed to be viewed by a Frames-capable
	browser.  If you see this message your browser is not
	Frames-capable."
    }
}

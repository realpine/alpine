#!/depot/path/tclsh

# This is a CGI script that creates some frames.

package require cgi

cgi_eval {
    source example.tcl
    cgi_input

    cgi_title "Frame example"

    # allow URL's of the form  ;# frame.cgi?example=....
    # so as to override default
    set example examples  ;# default is the examples page itself!!
    catch {cgi_import example}

    cgi_frameset rows=100,* {
	cgi_frame =$CGITCL
	cgi_frameset cols=200,* {
	    cgi_frame =examples.cgi?framed=yes
	    cgi_frame script=$example.cgi
	}
    }
    cgi_noframes {
	cgi_h1 "uh oh"

	p "This document is designed to be viewed by a Frames-capable
	browser.  If you see this message your browser is not
	Frames-capable."
    }
}

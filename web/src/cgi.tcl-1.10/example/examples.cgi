#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl

    input

    title "cgi.tcl examples"

    # use targets if we are framed
    if {0==[catch {import framed}]} {
	set target "target=script"
    } else {
	set target ""
    }

    # create a hyperlink to run a script
    proc run {name} {
	url $name.cgi [cgi $name] [uplevel {set target}]
    }

    # create hyperlink to show script source
    proc display {name} {
	url $name.cgi [cgi display scriptname=$name.cgi] [uplevel {set target}]
    }

    body bgcolor=#d0a0a0 text=#000000 {
	p "These are examples of cgi.tcl, a CGI support library for Tcl
	programmers.  If you would like to show off what you've
	written with cgi.tcl, give me an html-formatted blurb and I'll
	add it to a page of [link realapps].  For more information,
	visit the [link CGITCL]."

	bullet_list {
	    li "[run cookie] - Cookie example.  Also see [run passwd-form] to see cookies in the context of a real application."
	    li "[run creditcard] - Check a credit card."
	    li "[run download] - Demonstrate file downloading.  Also see [run upload]."
	    li "[run echo] - Echo everything - good for debugging forms."
	    li "[run error] - Error handling example."
	    li "[run evaljs] - Evaluate an expression using JavaScript."
	    li "[run examples] - Generate the page you are now reading."
	    li "[run form-tour] and [display form-tour-result] - Show
	    many different form elements and the backend to process them."
	    li "[run format-tour] - Demonstrate many formats."
	    li "[run frame] - Framed example (of this page)."
	    li "[url "image.cgi" [cgi display-in-frame [cgi_cgi_set scriptname \
		    image.cgi]] $target] - Produce a raw image."
	    li "[run img] - Examples of images embedded in a page."
	    li "[run kill] - Allow anyone to kill runaway CGI processes."
	    li "[display oratcl] - Use [link Oratcl] to query an Oracle database."
	    li "[run nistguest] - Specialized guestbook"
	    li "[run parray] - Demonstrate parray (print array elements)."
	    li "[run passwd-form] and [display passwd] - Form for
	    changing a password and its backend.  Note that this CGI script
	    is [bold not] setuid because it is written using [link Expect].
	    The script also demonstrates a nice use of cookies."
	    li "[run push] - Demonstrate server-push."
	    li "[run rm] - Allow anyone to remove old CGI files from /tmp."
	    li "[run stopwatch] - A stopwatch written as a Tcl applet."
	    li "[display unimail] - A universal mail backend that mails the
	    values of any form back to the form owner."
	    li "[run upload] - Demonstrate file uploading.  Also see [run download]."
	    li "[run utf] - Demonstrate UTF output."
	    li "[run validate] - Validate input fields using JavaScript."
	    li "[run vclock] - Lincoln Stein's virtual clock.
	    This was the big example in his CGI.pm paper.  Examine the
	    source to [eval url [list "his Perl version"] \
	    [cgi display scriptname=vclock.pl] $target] or compare them
	    [url "side by side" [cgi vclock-src-frame] target=script2]."
	    li "[run visitor] - Example of a visitor counter."
	    li "[run version] - Show version information."
	    li "[run vote] - Vote for a quote."
	    li "[run display] - Display a CGI script with clickable
	    source commands.  Not a particularly interesting application - 
	    just a utility to help demo these other CGI scripts!  But it is
	    written using cgi.tcl so it might as well be listed here."
	}
    }
}

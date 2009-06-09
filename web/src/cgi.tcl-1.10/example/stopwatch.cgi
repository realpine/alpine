#!/depot/path/tclsh

# This is a CGI script that demonstrates a simple Tcl applet

package require cgi

source example.tcl

set srcdir  http://www.nist.gov/mel/div826/src/stopwatch
set plugins http://www.sunlabs.com/research/tcl/plugin

cgi_link source      "source"                   $srcdir/stopwatch.tcl.html"
cgi_link gz          "complete distribution"    $srcdir/stopwatch.tar.gz
cgi_link moreplugins "More info on Tcl plugins" $plugins
cgi_link homepage    "homepage"                 $EXPECT_HOST/stopwatch

cgi_eval {

    cgi_input

    cgi_head {
	cgi_title "Stopwatch implemented via Tcl applet"
    }
    cgi_body {
	h3 "Description"

	p "This tclet provides a stopwatch.  I wrote it to help me
	time talks and individual slides within a talk.  Stopwatch can
	also be run as a Tk script outside the browser - which is the
	way I normally use it.  If you want to use it outside the browser, grab the distribution from the stopwatch [cgi_link homepage]."

	h3 "Directions"

	p {Press "start" to start the stopwatch and "stop" to stop it.
	"zero" resets the time.  You can also edit/cut/paste the time
	by hand and set it to any valid time.  A second timer is
	provided as well.  It works just like a normal lap timer.}

	cgi_embed http://www.nist.gov/mel/div826/src/stopwatch/stopwatch.tcl \
		450x105

	p "This is the first Tclet I've ever written.  Actually, I
	just took an existing Tk script I had already written and
	wrapped it in an HTML page.  It took about 30 minutes to write
	the original Tk script (about 80 lines) and 1 minute to embed
	it in an HTML page."

	p "Stopwatch is not intended for timing less than one second
	or longer than 99 hours.  It's easy to make make it show more
	but the code doesn't do it as distributed and I have no
	interest in adding more and more features until it reads mail.
	It's just a nice, convenient stopwatch."

	h3 "For more info"

	cgi_bullet_list {
	    cgi_li "Stopwatch [cgi_link homepage]."
	    cgi_li "Stopwatch [cgi_link source] and [cgi_link gz]."
	    cgi_li "[cgi_link moreplugins]."
	}
    }
}



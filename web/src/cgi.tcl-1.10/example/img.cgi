#!/depot/path/tclsh

# This is a CGI script that shows how to do simple images.

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "Images"

    set NIST_IMAGES http://www.nist.gov/images

    cgi_imglink ball $NIST_IMAGES/ball.gif alt=ball
    cgi_imglink birdies $NIST_IMAGES/brd-ln.gif alt=birdies
    cgi_imglink daemon $NIST_IMAGES/bsd_daemon.gif "alt=Kirk McKusick's BSD deamon"

    # use white background because some of these images require it
    cgi_body bgcolor=#ffffff text=#00b0b0 {

	p "Here are some birdies:[cgi_imglink birdies]"
	p "and here is your basic ball [cgi_imglink ball] and here is
		the BSD daemon [cgi_imglink daemon]."

	p "I like using the same picture"
	p "[cgi_imglink ball] over"
	p "[cgi_imglink ball] and over"
	p "[cgi_imglink ball] and over"
	p "[cgi_imglink ball] so I use the cgi_imglink command to make it easy."

	proc ball {} {return [cgi_imglink ball]}

	p "[cgi_imglink birdies]"

	p "[ball]I can make it even easier [ball] by making a ball
		procedure [ball] which I've just done. [ball] You could
		tell, eh? [ball]"
    }
}

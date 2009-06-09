#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl

    set boundary "ThisRandomString"

    cgi_http_head {
	cgi_content_type "multipart/x-mixed-replace;boundary=$boundary"

	puts \n--$boundary
	cgi_content_type
    }

    cgi_title "Multipart example - 1st page"
    cgi_body {
	h4 "This is an example of [italic server-push] as implemented
	by the multipart MIME type.  In contrast with client-pull, push
	leaves the connection open and the CGI script remains in control
	as to send more information.  The additional information can
	be anything - this example demonstrates an entire page being
	replaced."
    }

    puts \n--$boundary
    after 5000

    cgi_content_type

    cgi_title "Multipart example - 2nd page"
    cgi_body {
	h4 "This page replaced the previous page with no action on the
	client side."
    }
}

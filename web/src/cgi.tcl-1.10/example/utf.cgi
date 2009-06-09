#!/local/bin/tclsh

# Test UTF encoding

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "Study utf encoding"
    cgi_html {
	cgi_body {
	    p "I'm going to be living on the Straüße."
	}
    }
}


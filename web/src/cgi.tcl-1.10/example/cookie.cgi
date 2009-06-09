#!/depot/path/tclsh

# This CGI script shows how to create a cookie.

package require cgi

cgi_eval {
    source example.tcl

    cgi_input

    cgi_http_head {
	cgi_content_type text/html
	if {0==[catch {cgi_import Name}]} {
	    cgi_export_cookie Name ;# expires=never
	    # For a persistent cookie, uncomment the "expires=never"
	} else {
	    catch {cgi_import_cookie Name}
	}
    }
    cgi_head {
	cgi_title "Cookie Form Example"
    }
    cgi_body {
	p "This form finds a value from a previous submission of \
		the form either directly or through a cookie."
	set Name ""

	if {0==[catch {cgi_import Name}]} {
	    p "The value was found from the form submission and the cookie
		has now been set.  Bookmark this form, surf somewhere else
		and then return to this page to get the value via the cookie."
	} elseif {0==[catch {cgi_import_cookie Name}]} {
	    p "The value was found from the cookie."
	} else {
	    p "No cookie is currently set.  To set a cookie, enter a value
		and press return."
	}

	cgi_form cookie {
	    puts "Value: "
	    cgi_text Name
	}
    }
}

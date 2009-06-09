#!/depot/path/tclsh

# This is a CGI script to present a form in which to change a password.

# This form doesn't actually have to be written as a CGI script,
# however, it is done so here to demonstrate the procedures described
# in the Tcl '96 paper by Don Libes.  You can find this same form
# written as static html in the example directory of the Expect
# package.

package require cgi

cgi_eval {
    source example.tcl
    source passwd.tcl

    cgi_input

    # Use a cookie so that if user has already entered name, don't make them
    # do it again.  If you dislike cookies, simply remove the next two
    # lines - cookie use here is simply a convenience for users.
    set login ""
    catch {cgi_import_cookie login}

    cgi_title "Change your login password"
    cgi_body {
	cgi_form passwd {
	    put "Username: "; cgi_text login size=16
	    password "Old" old
	    password "New" new1
	    password "New" new2

	    p "(The new password must be entered twice to avoid typos.)"

	    cgi_submit_button "=Change password"
	    cgi_reset_button
	}
    }
}

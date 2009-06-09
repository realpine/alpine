#!/depot/path/expect --

# This is a CGI script to process requests created by the accompanying
# passwd.html form.  This script is pretty basic, although it is
# reasonably robust.  (Purposely intent users can make the script bomb
# by mocking up their own HTML form, however they can't expose or steal
# passwords or otherwise open any security holes.)  This script doesn't
# need any special permissions or ownership.
#
# With a little more code, the script can do much more exotic things -
# for example, you could have the script:
#
# - telnet to another host first (useful if you run CGI scripts on a
#   firewall), or
#
# - change passwords on multiple password server hosts, or
# 
# - verify that passwords aren't in the dictionary, or
# 
# - verify that passwords are at least 8 chars long and have at least 2
#   digits, 2 uppercase, 2 lowercase, or whatever restrictions you like,
#   or
#
# - allow short passwords by responding appropriately to passwd
#
# and so on.  Have fun!
#
# Don Libes, NIST

package require cgi

cgi_eval {
    source example.tcl
    source passwd.tcl

    cgi_input

    # Save username as cookie (see comment in passwd-form script) so that the
    # next time users load the form, their username will already be filled in.
    # If cookies bother you, simply remove this entire cgi_http_head command.
    cgi_http_head {
	cgi_content_type text/html
	if {0==[catch {cgi_import login}]} {
	    cgi_export_cookie login expires=never
	}
    }

    cgi_title "Password Change Acknowledgment"
    cgi_body {
	if {(![info exists login])
	    || [catch {cgi_import old}]
	    || [catch {cgi_import new1}]
	    || [catch {cgi_import new2}]} {
	    errormsg "This page has been called with input missing.  Please \
		    visit this form by filling out the password change \
 		    request form."
	    return
	}

	# Prevent user from sneaking in commands (albeit under their own uid).
	if {[regexp "\[^a-zA-Z0-9]" $login char]} {
	    errormsg "Illegal character ($char) in username."
	    return
	}

	log_user 0

	# Need to su first to get around passwd's requirement that
	# passwd cannot be run by a totally unrelated user.  Seems
	# rather pointless since it's so easy to satisfy, eh?

	# Change following line appropriately for your site.
	# (We use yppasswd, but you might use something else.)
	spawn /bin/su $login -c "/bin/yppasswd $login"
	# This fails on SunOS 4.1.3 (passwd says "you don't have a
	# login name") so run on (or telnet first to) host running
	# SunOS 4.1.4 or later.

	expect {
	    -re "Unknown (login|id):" {
		errormsg "unknown user: $login"
		return
	    } default {
		errormsg "$expect_out(buffer)"
		return
	    } "Password:"
	}
	send "$old\r"
	expect {
	    "unknown user" {
		errormsg "unknown user: $login"
		return
	    } "Sorry" {
		errormsg "Old password incorrect"
		return
	    } default {
		errormsg "$expect_out(buffer)"
		return
	    } "Old password:"
	}
	send "$old\r"
	expect "New password:"
	send "$new1\r"
	expect "New password:"
	send "$new2\r"
	expect -re (.+)\r\n {
	    set error $expect_out(1,string)
	}
	close
	wait

	if {[info exists error]} {
	    errormsg "$error"
	} else {
	    successmsg "Password changed successfully."
	}
    }
}


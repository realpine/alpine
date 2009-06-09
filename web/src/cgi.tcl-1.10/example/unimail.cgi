#!/depot/path/tclsh

# This script is a universal mail backend.  It's handy for creating
# forms that simply email all their elements to someone else.  You
# wouldn't use it in a fancy application, but non-programmers like it
# since they can use it to process forms with no CGI scripting at all.
#
# To use, make your form look something like this:
#
#    <form action="http://ats.nist.gov/cgi-bin/cgi.tcl/unimail.cgi" method=post>
#      <input type=hidden name=mailto value="YOUR EMAIL ADDRESS HERE">
#      ...rest of your form...
#    </form>
#
# Note: You can use our action URL to try this out, but please switch
# to using your own local unimail script for production use.  Thanks!
#
# Author: Don Libes, NIST

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "Universal mail backend"

    cgi_body {
	if {[catch cgi_input errormsg]} {
	    h2 "Form Error"
	    p "An error was detected in the form.  Please send the
	    following diagnostics to the form author."
	    cgi_preformatted {puts $errormsg}
	    return
	}
	if {[catch {cgi_import mailto}]} {
	    h2 "Error: No mailto variable in form."
	    return
	}
	if {![info exists env(HTTP_REFERER)]} {
	    set env(HTTP_REFERER) "unknown"
	}
	cgi_mail_start $mailto
	cgi_mail_add "Subject: submission from web form: $env(HTTP_REFERER)"
	cgi_mail_add
	catch {cgi_mail_add "Remote addr: $env(REMOTE_ADDR)"}
	catch {cgi_mail_add "Remote host: $env(REMOTE_HOST)"}

	foreach item [cgi_import_list] {
	    cgi_mail_add "$item: [cgi_import $item]"
	}
	cgi_mail_end

        if {[catch {cgi_import thanks}]} {
            set thanks [cgi_buffer {h2 "Thanks for your submission."}]
        }
        cgi_put $thanks
    }
}

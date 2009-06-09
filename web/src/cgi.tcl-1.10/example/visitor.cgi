#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "Visitor count example"

    cgi_body {
	cgi_put "This page demonstrates how easy it is to do visitor counts."

	cgi_uid_check http

	cgi_form visitor {
	    set cfname "$DATADIR/visitor.cnt"

	    if {[catch {set fid [open $cfname r+]} errormsg]} {
		h4 "Couldn't open $cfname to maintain visitor count: $errormsg"
		return
	    }
	    gets $fid count
	    seek $fid 0 start
	    puts $fid [incr count]
	    close $fid
	    h4 "You are visitor $count.  Revisit soon!"
	    cgi_submit_button "=Revisit"
	}
    }
}

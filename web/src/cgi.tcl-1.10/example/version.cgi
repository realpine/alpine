#!/depot/path/tclsh

# This is a CGI script that displays some version information that I
# find useful for debugging.

set v [package require cgi]

proc row {var val} {
    table_row {
	td $var
	td $val
    }
}

cgi_eval {
    source example.tcl

    title "Version info"

    cgi_body {
	table border=2 {
	    row "cgi.tcl" $v
	    row "Tcl" [info patchlevel]
	    row "uname -a" [exec uname -a]
	    catch {row "SERVER_SOFTWARE" $env(SERVER_SOFTWARE)}
	    catch {row "HTTP_USER_AGENT" $env(HTTP_USER_AGENT)}
	    catch {row "SERVER_PROTOCOL" $env(SERVER_PROTOCOL)}
	}
    }
}

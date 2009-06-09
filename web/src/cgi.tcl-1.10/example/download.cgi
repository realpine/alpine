#!/depot/path/tclsh

package require cgi

set msg "Very funny, Scotty.  Now beam down my clothes."
set filename "scotty.msg"

cgi_eval {
    source example.tcl

    cgi_input

    if {[catch {cgi_import style}]} {
	cgi_title "download example"
	body {
	    cgi_suffix ""
	    form download.cgi/$filename {
		puts "This example demonstrates how to force files to be
		downloaded into a separate file via the popup file browser."
		br
		puts "Download data"
		submit_button "style=in window"
		submit_button "style=in file using popup file browser"
	    }
	}
    } else {
	if {[regexp "in window" $style]} {
	    title "Display data in browser window"
	} else {
	    cgi_http_head {
		content_type application/x-download
	    }
	}
	puts $msg
    }
}

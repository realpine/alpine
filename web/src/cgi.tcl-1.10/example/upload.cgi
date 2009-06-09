#!/depot/path/tclsh

# This is a CGI script that demonstrates file uploading.

package require cgi

cgi_eval {
    source example.tcl

    proc showfile {v} {
	catch {
	    set server [cgi_import_file -server $v]
	    set client [cgi_import_file -client $v]
	    set type   [cgi_import_file -type $v]
	    if {[string length $client]} {
		h4 "Uploaded: $client"
		if {0 != [string compare $type ""]} {
		    h4 "Content-type: $type"
		}
		cgi_import showList
		foreach show $showList {
		    switch $show {
			"od -c" - "cat" {
			    h5 "Contents shown using $show"
			    cgi_preformatted {puts [eval exec $show [list $server]]}
			}
		    }
		}
	    }
	    exec /bin/rm -f $server
	}
    }

    cgi_input

    cgi_head {
	cgi_title "File upload demo"
    }
    cgi_body {
	if {[info tcl] < 8.1} {
	    h4 "Warning: This script can not perform binary uploads because the server is running a pre-8.1 Tcl ([info tcl])."
	}

	showfile file1
	showfile file2

	cgi_form upload enctype=multipart/form-data {
	    p "Select up to two files to upload"
	    cgi_file_button file1; br
	    cgi_file_button file2; br
	    checkbox "showList=cat" checked;
	    put "show contents using cat"   ;br
	    checkbox "showList=od -c"
	    put "show contents using od -c" ;br
	    cgi_submit_button =Upload
	}
    }
}


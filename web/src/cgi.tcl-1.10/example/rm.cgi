#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl

    cgi_input

    cgi_title "Remove old CGI files from /tmp"

    cgi_body {
	if {0==[catch {cgi_import FileList}]} {
	    catch {cgi_import Refresh;set FileList {}}
	    h4 "If this were not a demo, the following commands would have been executed:"
	    foreach File $FileList {
		# prevent deletion of this dir or anything outside it
		set File [file tail $File]
		switch $File . - .. - "" {
		    h3 "Illegal filename: $File"
		    continue
		}

		# to undemoize this and allow files to be killed,
		# remove h5 and quotes
		if {[catch {h5 "file delete -force /tmp/$File"} msg]} {
		    h4 "$msg"
		}
	    }
	}

	cgi_form rm {
	    set f [open "|/bin/ls -Alt /tmp" r]
	    table border=2 {
		table_row {
		    table_data {puts "rm -rf"}
		    table_data {cgi_preformatted {puts "permissions ln owner    group       size     date     filename"}}
		}
		while {-1 != [gets $f buf]} {
		    if {![regexp " http " $buf]} continue
		    table_row {
			table_data {
			    regexp ".* (\[^ ]+)$" $buf dummy File
			    cgi_checkbox FileList=$File
			}
			table_data {
			    cgi_preformatted {puts $buf}
			}
		    }
		}
		
	    }

	    submit_button "=Removed selected files"
	    submit_button "Refresh=Refresh listing"
	    reset_button
	}
    }
}

#!/depot/path/tclsh

# This CGI script uses JavaScript to validate a form before submission.

package require cgi

cgi_eval {
    source example.tcl

    cgi_input

    cgi_head {
	title "Using JavaScript to validate a form before submission"

	javascript {
	    puts {
		function odd(num) {
		    if (num.value % 2 == 0) {
			alert("Please enter an odd number!")
			num.value = ""
			return false
		    }
		    return true
		}
	    }
	}
	noscript {
	    puts "Sorry - your browser doesn't understand JavaScript."
	}
    }

    set rownum 0
    proc row {msg {event {}}} {
	global rownum

	incr rownum
	table_row {
	    table_data nowrap {
		put  "Odd number: "
		text num$rownum= size=4 $event
	    }
	    table_data {
		puts $msg
	    }
	}
    }

    cgi_body {
	set more ""
	if {0 == [catch {import num3}]} {
	    set count [scan $num3 %d num]
	    if {($count != 1) || ($num % 2 == 0)} {
		p "Hey, you didn't enter an odd number!"
	    } else {
		p "Thanks for entering odd numbers!"
		set more " more"
	    }
	}

	puts "Please enter$more odd numbers - thanks!"

	cgi_form validate "onSubmit=return odd(this.num2)" {
	    table {
		row "This number will be validated when it is entered." onChange=odd(this.form.num1)
		row "This number will be validated when the form is submitted."
		row "This number will be validated after the form is submitted."
	    }
	    submit_button =Submit
	}

	h5 "Note: JavaScript validation should always be accompanied
	by validation in the backend (CGI script) since browsers
	cannot be relied upon to have JavaScript enabled (or supported
	in the first place).  Sigh."
    }
}

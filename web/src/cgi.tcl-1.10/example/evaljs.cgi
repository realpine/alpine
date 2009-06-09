#!/depot/path/tclsh

# This CGI script uses JavaScript to evaluate an expression.

package require cgi

cgi_eval {
    source example.tcl

    cgi_head {
	title "Using JavaScript to evaluate an expression"

	javascript {
	    puts {
		function compute(f) {
		    f.result.value = eval(f.expr.value)
		}
	    }
	}
	noscript {
	    puts "Sorry - your browser doesn't understand JavaScript."
	}
    }

    cgi_body {
	cgi_form dummy {
	    cgi_unbreakable {
		cgi_button "Evaluate" onClick=compute(this.form)
		cgi_text expr=Math.sqrt(2)*10000
		puts "="
		cgi_text result=
	    }
	    p "Feel free to enter and evaluate your own JavaScript expression."
	}
    }
}

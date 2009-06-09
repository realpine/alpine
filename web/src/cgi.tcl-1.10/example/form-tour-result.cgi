#!/depot/path/tclsh

# This is a CGI script that shows the result of the form tour.

package require cgi

cgi_eval {
    source example.tcl

    cgi_input
    cgi_title "Form Element Tour Results"

    cgi_body {
	h4 "This is a report of variables set by the form tour."

	if {0!=[catch {cgi_import Foo0}]} {
	    h5 "Error: It appears that you have invoked this script
	    without going through [cgi_url "the intended form" [cgi_cgi form-tour]]."
	    cgi_exit
	}

	catch {
	    cgi_import Map.x
	    cgi_import Map.y
	    puts "image button coordinates = ${Map.x},${Map.y}[nl]"
	}

	catch {
	    cgi_import Action
	    puts "submit button Action=$Action[nl]"
	    br
	}

	foreach x {version A B C D} {
	    catch {
		cgi_import $x
		puts "radio button \"$x\": [set $x][nl]"
	    }
	}
	catch {
	    cgi_import VegieList
	    puts "checkbox Vegielist = $VegieList[nl]"
	}
	for {set i 0} {$i<=6} {incr i} {
	    set var Foo$i
	    cgi_import $var
	    puts "text $var:"
	    cgi_preformatted {
		puts [set $var]
	    }
	}
	for {set i 0} {$i<=9} {incr i} {
	    set var Foo1$i
	    cgi_import $var
	    puts "textvar $var:"
	    cgi_preformatted {
		puts [set $var]
	    }
	}
	catch {
	    cgi_import Foo
	    puts "select pull-down Foo: $Foo[nl]"
	}
	catch {
	    cgi_import FooList
	    puts "select scrolled list FooList: $FooList[nl]"
	}
    }
}

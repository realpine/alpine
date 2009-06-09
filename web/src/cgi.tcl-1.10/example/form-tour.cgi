#!/depot/path/tclsh

# This is a CGI script that shows a selection of form elements
# This script doesn't actually DO anything.

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "A Tour of Form Elements"

    cgi_body {
	form form-tour-result {
	    h4 "Samples of image button"
	    cgi_put "image as button"
	    cgi_image_button "=http://www.nist.gov/msidimages/title.gif"
	    br
	    cgi_put "image as button (and will return coords)"
	    cgi_image_button "Map=http://www.nist.gov/public_affairs/gallery/fireseat.jpg"

	    h4 "Samples of submit button"
	    cgi_submit_button
	    cgi_submit_button ="Submit"
	    cgi_submit_button "=Submit Form"
	    cgi_submit_button "Action=Pay Raise"

	    h4 "Samples of reset button"
	    cgi_reset_button
	    cgi_put "Default Reset Button"
	    br
	    cgi_reset_button "Not the Default Reset Button"
	    cgi_put "Not the Default Reset Button"

	    h4 "Samples of radio button"
	    cgi_radio_button "version=1"
	    cgi_put "Version 1"
	    br
	    cgi_radio_button "version=2"
	    cgi_put "Version 2"
	    br
	    cgi_radio_button "version=3" checked
	    cgi_put "Version 3"

	    br
	    foreach x {A B C D} {
		cgi_radio_button "$x=" checked_if_equal=B
		cgi_put "$x"
		br
	    }

	    h4 "Samples of checkbox"
	    cgi_checkbox VegieList=carrot
	    cgi_put "Carrot"
	    br
	    cgi_checkbox VegieList=rutabaga checked
	    cgi_put "Rutabaga"
	    br
	    cgi_checkbox VegieList=
	    cgi_put "Vegie"

	    h4 "Samples of textentry"
	    set Foo0 "value1"
	    set Foo1 "value1"
	    cgi_text Foo0
	    br;cgi_text Foo1=
	    br;cgi_text Foo2=value2
	    br;cgi_text Foo3=value2 size=5
	    br;cgi_text Foo4=value2 size=5 maxlength=10
	    br;cgi_text Foo5=value2 size=10 maxlength=5
	    br;cgi_text Foo6=value2 maxlength=5

	    h4 "Samples of textarea"

	    set value "A really long line so that we can compare the\
		    effect of wrap options."

	    set Foo10 "value1"
	    set Foo11 "value1"
	    cgi_textarea Foo10
	    br;cgi_textarea Foo11=
	    br;cgi_textarea Foo12=$value
	    br;cgi_textarea Foo13=$value         rows=3
	    br;cgi_textarea "Foo14=default wrap" rows=3 cols=7
	    br;cgi_textarea Foo15=wrap=off       rows=3 cols=7 wrap=off
	    br;cgi_textarea Foo16=wrap=soft      rows=3 cols=7 wrap=soft
	    br;cgi_textarea Foo17=wrap=hard      rows=3 cols=7 wrap=hard
	    br;cgi_textarea Foo18=wrap=physical  rows=3 cols=7 wrap=physical
	    br;cgi_textarea Foo19=wrap=virtual   rows=3 cols=7 wrap=virtual

	    h4 "Samples of select as pull-down menu"
	    cgi_select Foo {
		cgi_option one selected
		cgi_option two
		cgi_option many value=hello
	    }

	    h4 "Samples of select as scrolled list"
	    cgi_select FooList multiple {
		cgi_option one selected
		cgi_option two selected
		cgi_option many 
	    }
	    br
	    cgi_select FooList multiple size=2 {
		cgi_option two selected
		cgi_option three selected
		cgi_option manymore
	    }
	    br
	    # choose "selected" dynamically
	    cgi_select FooList multiple size=5 {
		foreach o [info comm] {
		    cgi_option $o selected_if_equal=exit
		}
	    }
	    h4 "Samples of isindex"
	}
	cgi_isindex
	cgi_isindex "prompt=Enter some delicious keywords: "
    }
}


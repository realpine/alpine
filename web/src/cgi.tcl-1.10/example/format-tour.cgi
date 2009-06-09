#!/depot/path/tclsh

# This is a CGI script that shows a selection of format elements

package require cgi

cgi_eval {
    source example.tcl

    cgi_title "A Tour of HTML Elements"
    cgi_body {

	definition_list {
	    term "term"
	    term_definition "definition of term"
	}

	h4 "menu list"
	menu_list {
	    li item1
	    li item2
	}

	h4 "directory list"
	directory_list {
	    li item1
	    li item2
	}

	h4 "a list item by itself"
	li "item"

	h4 "number list (roman, starting from 4)"
	number_list type=i value=4 {
	    li "first element"
	    li "second"
	    li value=9 "third, start numbering from 9"
	    li type=A "fourth, switch to upper-arabic"
	}

	h4 "bullet list"
	bullet_list {
	    p "plain text"
	    li "plain item"
	    h4 "nested list (disc, starting from 4)"
	    bullet_list type=disc value=4 {
		li "first element"
		li "second"
		li type=circle "third, type=circle"
		li type=square "fourth, type=square"
		li "fifth, should remain square"
	    }
	}

	h4 "Character formatting samples"
	cgi_put "[bold bold]\
		[italic italic]\
		[underline underline]\
		[strikeout strikeout]\
		[subscript subscript]\
		[superscript superscript]\
		[typewriter typewriter]\
		[blink blink]
	[emphasis emphasis]\
		[strong strong]\
		[cite cite]\
		[sample sample]\
		[keyboard keyboard]\
		[variable variable]\
		[definition definition]\
		[big big]\
		[small small]\
		[font color=#4499cc "color=#4499cc"]\
		"
	for {set i 1} {$i<8} {incr i} {
	    puts [cgi_font size=$i "size=$i"]
	}

	h4 "Paragraph formatting samples"

	cgi_h1 h1
	cgi_h2 h2
	cgi_h3 h3
	cgi_h4 h4
	cgi_h5 h5
	cgi_h6 h6
	cgi_h7 "h7 (beyond the spec, what the heck)"
	cgi_h6 align=right "right-aligned h6"
	cgi_p align=right "right-aligned paragraph"
	cgi_put put
	cgi_blockquote "blockquote"
	cgi_address address
	cgi_division {
	    puts "division"
	}
	cgi_preformatted {
	    puts "preformatted"
	}
    }
}


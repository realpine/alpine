#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl

    set cardtypes {
	"American Express"
	"Carte Blanche"
	"Diners Card"
	"Discover"
	"Enroute"
	"JCB"
	"Mastercard"
	"Novus"
	"Visa"
    }

    # My own version of the LUHN check
    proc LUHNFormula {cardnumber} {
	if {0==[regexp "(.*)(.)$" $cardnumber dummy cardnumber check]} {
	    user_error "No card number entered"
	}
	set evenodd [expr [string length $cardnumber] % 2]

	set sum 0
	foreach digit [split $cardnumber ""] {
	    incr sum $digit
	    if {$evenodd} {
		incr sum [lindex {0 1 2 3 4 -4 -3 -2 -1 0} $digit]
	    }
	    set evenodd [expr !$evenodd]
	}
	set computed [expr {(10 - ($sum % 10)) % 10}]
	if {$computed != $check} {
	    user_error "Invalid card number.  (Failed LUHN test - try changing last digit to $computed.)"
	}
    }    

    # generate digit patterns of length n
    proc d {n} {
	for {set i 0} {$i < $n} {incr i} {
	    append buf {[0-9]}
	}
	return $buf
    }

    cgi_input

    cgi_title "Check Credit Card"

    cgi_body {
	if {0 == [catch {cgi_import cardtype}]} {
	    if {[catch {cgi_import cardnumber}]} {
		user_error "No card number entered"
	    }
	    # Save original version for clearer diagnostics
	    set originalcardnumber [cgi_quote_html $cardnumber]

	    if {[catch {cgi_import expiration}]} {
		user_error "You must enter an expiration."
	    }		
	    if {-1 == [lsearch $cardtypes $cardtype]} {
		user_error "Unknown card type: $cardtype"
	    }

	    # Remove any spaces or dashes in card number
	    regsub -all "\[- ]" $cardnumber "" cardnumber

	    # Make sure that only digits are left
	    if {[regexp "\[^0-9]" $cardnumber invalid]} {
		user_error "Invalid character ([cgi_quote_html $invalid]) in credit card number: $originalcardnumber"
	    }

	    if {$cardtype != "Enroute"} {
		LUHNFormula $cardnumber
	    }

	    # Verify correct length and prefix for each card type
	    switch $cardtype {
		Visa {
		    regexp "^4[d 12]([d 3])?$" $cardnumber match
		} Mastercard {
		    regexp "^5\[1-5][d 14]$" $cardnumber match
		} "American Express" {
		    regexp "^3\[47][d 13]$" $cardnumber match
		} "Diners Club" {
		    regexp "^3(0\[0-5]|\[68][d 1])[d 11]$" $cardnumber match
		} "Carte Blanche" {
		    regexp "^3(0\[0-5]|\[68][d 1])[d 11]$" $cardnumber match
		} Discover {
		    regexp "^6011[d 12]$" $cardnumber match
		} Enroute {
		    regexp "^(2014|2149)[d 11]$" $cardnumber match
		} JCB {
		    regexp "^(2131|1800)[d 11]$" $cardnumber match
		    regexp "^3(088|096|112|158|337|528)[d 12]$" $cardnumber match
		} Novus {
		    if {[string length $cardnumber] == 16} {
			set match 1
		    }
		}
	    }

	    if 0==[info exists match] {
		user_error "Invalid card number: $originalcardnumber"
	    }
	    h3 "Your card appears to be valid.  Thanks!"
	    return
	}

	cgi_form creditcard {
	    h3 "Select a card type, enter the card number and expiration."
	    puts "Card type: "
	    cgi_select cardtype {
		foreach t $cardtypes {
		    cgi_option $t
		}
	    }
	    puts "[nl]Card number: "
	    cgi_text cardnumber=
	    puts "(blanks and dashes are ignored)"
	    
	    puts "[nl]Expiration: "
	    cgi_text expiration=

	    br
	    submit_button "=Confirm purchase"
	    reset_button
	    h5 "This script will perform all of the known syntactic
	    checks for each card type but will not actually contact
	    a credit bureau.  The expiration field is not presently
	    checked at all."
	}
    }
}

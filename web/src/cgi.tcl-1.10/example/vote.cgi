#!/depot/path/tclsh

package require cgi

cgi_eval {
    source example.tcl
    cgi_input

    cgi_title "Vote for a t-shirt design!"

    cgi_uid_check http

    set BarURL $EXPECT_ART/pink.gif

    set Q(filename) "$DATADIR/vote.cnt"

    proc votes_read {} {
	global Q

	set Q(Max) 0
	set Q(Votes) 0
	set Q(Indices) ""

	set fid [open $Q(filename) r]
	while {-1!=[gets $fid i]} {
	    gets $fid buf
	    foreach "Q(Votes,$i) Q(Entry,$i) Q(Name,$i) Q(Email,$i)" $buf {}
	    lappend Q(Indices) $i
	    set Q(Unvotable,$i) [catch {incr Q(Votes) $Q(Votes,$i)}]
	    set Q(Max) $i
	}
	close $fid
    }

    proc votes_write {} {
	global Q

	# No file locking (or real database) handy so we can't guarantee that
	# simultaneous votes aren't dropped, but we'll at least avoid
	# corruption of the vote file by working on a private copy.
	set tmpfile $Q(filename).[pid]
	set fid [open $tmpfile w]
	foreach i $Q(Indices) {
	    set data [list $Q(Votes,$i) $Q(Entry,$i) $Q(Name,$i) $Q(Email,$i)]
	    # simplify votes_read by making each entry a single line
	    regsub -all \n $data <br> data
	    puts $fid $i\n$data
	}
	close $fid
	exec mv $tmpfile $Q(filename)
    }

    proc vote_other_show {} {
	global Q

	h3 "Other suggestions"
	table border=2 {
	    table_row {
		th width=300 "Entry"
		th width=300 "Judge's Comments"
	    }
	    foreach i $Q(Indices) {
		if {!$Q(Unvotable,$i)} continue
		table_row {
		    td width=300 "$Q(Entry,$i)"
		    td width=300 "$Q(Votes,$i)"
		}
	    }
	}
    }
	

    cgi_body {
	votes_read

	if {[regexp "Vote(\[0-9]+)" [cgi_import_list] dummy i]} {
	    if {[catch {incr Q(Votes,$i)}]} {
		user_error "There is no such entry to vote for."
	    }
	    incr Q(Votes)
	    votes_write

	    h3 "Thanks for voting!  See you at the next Tcl conference!"
	    set ShowVotes 1
	}
	catch {cgi_import ShowVotes}
	if {[info exists ShowVotes]} {
	    table border=2 {
		table_row {
		    th width=300 "Entry"
		    th "Votes"
		    th width=140 "Percent" ;# 100 + room for pct
		}
		foreach i $Q(Indices) {
		    if {!$Q(Unvotable,$i)} {
			table_row {
			    td width=300 "$Q(Entry,$i)"
			    td align=right "$Q(Votes,$i)"
			    table_data width=140 {
				table {
				    table_row {
					set pct [expr 100*$Q(Votes,$i)/$Q(Votes)]
					# avoid 0-width Netscape bug
					set pct_bar [expr $pct==0?1:$pct]
					td [img $BarURL align=left width=$pct_bar height=15]
					td $pct
				    }
				}
			    }
			}
		    }
		}
	    }
	    form vote {
		submit_button "=Submit entry or vote"
		submit_button "ShowVotes=Show votes"
	    }
	    vote_other_show
	    return
	}

	if 0==[catch {import Entry}] {
	    if {[string length $Entry] == 0} {
		user_error "No entry found."
	    }
	    if {[string length $Entry] > 500} {
		user_error "Your entry is too long.  Keep it under 500 characters!"
	    }
	    set Name ""
	    catch {import Name}
	    if 0==[string length $Name] {
		user_error "You must supply your name.  How are we going to know who you are if you win?"
	    }
	    set Email ""
	    catch {import Email}
	    if 0==[string length $Email] {
		user_error "You must supply your email.  How are we going to contact you if you win?"
	    }
		
	    set i [incr Q(Max)]
	    set Q(Entry,$i) $Entry
	    set Q(Name,$i) $Name
	    set Q(Email,$i) $Email
	    set Q(Votes,$i) 1
	    lappend Q(Indices) $i

	    votes_write

	    h3 "Thanks for your new entry!"
	    p "No need to go back and vote for it - a vote has already
	    been recorded for you."
	    form vote {
		submit_button "=Submit another entry or vote"
		submit_button "ShowVotes=Show votes"
	    }
	    return
	}

	p "Vote for what will go on the next Tcl conference t-shirt!  (Feel free to vote for several entries.)"

	cgi_form vote {
	    table border=2 {
		foreach i $Q(Indices) {
		    if {$Q(Unvotable,$i)} continue
		    table_row {
			table_data {
			    cgi_submit_button Vote$i=Vote
			}
			td "$Q(Entry,$i)"
		    }
		}
	    }
	    br
	    cgi_submit_button "ShowVotes=Just show me the votes"
	    hr
	    p "The author of the winning entry gets fame and glory (and a free t-shirt)!  Submit a new entry:"
	    cgi_text Entry= size=80
	    p "Entries may use embedded HTML.  Images or concepts are fine - for artwork, we have the same artist who did the [url "'96 conference shirt" $MSID_STAFF/libes/t.html]).  The [url judges mailto:tclchairs@usenix.org] reserve the right to delete entries.  (Do us a favor and use common sense and good taste!)"
	    puts "Name: "
	    cgi_text Name=
	    br
	    puts "Email: "
	    cgi_text Email=
	    br
	    cgi_submit_button "=Submit new entry"

	    vote_other_show
	}
    }
}

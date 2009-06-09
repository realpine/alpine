#!./tclsh

# and suck in any global config
source ./alpine.tcl

# figure out SSL defaults
if {[info exists _wp(ssl_default)] && $_wp(ssl_default) == 0} {
  set ssl 0
} else {
  set ssl 1
  if {[info exists _wp(ssl_safe_domains)]} {
    if {[info exists env(REMOTE_HOST)]} {
      foreach d $_wp(ssl_safe_domains) {
	regsub -all {\.} $d {\.} d
	if {[regexp -nocase "$d\$" $env(REMOTE_HOST)]} {
	  set ssl 0
	  break
	}
      }
    }

    if {$ssl && [info exists env(REMOTE_ADDR)]} {
      regexp {([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)} $env(REMOTE_ADDR) dummy a b c d
      set h "$d.$c.$b.$a.in-addr.arpa"
      if {[catch {exec nslookup -q=PTR $h "2&>1"} pr] == 0} {
	foreach l [split $pr \n] {
	  if {[regexp -nocase "^$h\[	\]*name = (.*)\$" $l dummy inaddr]} {
	    break
	  }
	}
      }

      if {[info exists inaddr]} {
	foreach d $_wp(ssl_safe_domains) {
	  if {[set n [expr {[string length $inaddr] - [string length $d] - 1}]] > 1
	  && [string compare [string tolower [string range $inaddr $n end]] [string tolower ".$d"]] == 0} {
	    set ssl 0
	    break
	  }
	}
      }
    }
  }

  if {$ssl && [info exists _wp(ssl_safe_addrs)]} {
    set ra [split $env(REMOTE_ADDR) .]
    foreach a $_wp(ssl_safe_addrs) {
      if {[llength $a] == 2} {
	set low [split [lindex $a 0] .]
	set hi [split [lindex $a 1] .]

	foreach a $ra b $low c $hi {
	  if {$b == $c} {
	    if {$a != $b} {
	      break
	    }
	  } else {
	    if {$a >= $b && $a <= $c} {
	      set ssl 0
	      break
	    }
	  }
	}
      } else {
	foreach a [split $a .] b $ra {
	  if {[string length $a]} {
	    if {$a != $b} {
	      break
	    }
	  } else {
	    set $ssl 0
	    break
	  }
	}
      }

      if {!$ssl} {
	break
      }
    }
  }
}

cgi_eval {

  cgi_http_head {
    WPStdHttpHdrs
  }

  cgi_html {

    cgi_head {
      WPStdHtmlHdr "Web Alpine - Logon"
      WPStdScripts $_wp(indexheight)

      cgi_javascript {
	if {[isIE]} {
	  cgi_puts "document.onkeypress = keyed;"
	} else {
	  cgi_put  "document.onkeydown = keyed;"
	  cgi_puts "document.captureEvents(Event.KEYDOWN|Event.KEYUP);"
	}
	cgi_put  "function keyed(e){"
	cgi_put  " var c = getKeyStr(e);"
	cgi_put  " if(c == '\\r') { document.Login.submit(); return false; }"
	if {![isIE]} {
	  cgi_put  "else return routeEvent(e);"
	}
	cgi_puts "}"
      }
    }

    cgi_body "bgcolor=#F0E68C" style="font-family: sans-serif" {
      cgi_puts [cgi_nbspace][cgi_nl]
      cgi_form session/logon method=post name=Login {
	cgi_table width=90% align=center border=0 {
	  cgi_table_row {
	    cgi_table_data align=center {
	      cgi_puts [cgi_font size=+2 "WebPine"]
	      cgi_br
	      cgi_puts [cgi_font size=+3 color=green "Older Versions WebPine"]
	      cgi_br
	      cgi_puts [cgi_font size=+3 color=red "(to be discontinued in the near future)"]
	      cgi_p
	    }
	  }

	  cgi_table_row {
	    cgi_table_data {
	      cgi_table border=0 cellspacing=15 cellpadding=1 width=100% bgcolor=white "style=\"border: 1px solid goldenrod\"" {
		cgi_table_row {
		  cgi_table_data align=center colspan=2 {
		    set intro "Welcome to WebPine"
		    if {[info exists env(REMOTE_USER)]} {
		      set conf [subst [lindex [lindex $_wp(hosts) 0] 2]]
		      set pldap [file join $_wp(bin) $_wp(pldap)]
		      if {[catch {exec $pldap -p $conf -u $env(REMOTE_USER)} pname] == 0 && [string length $pname]} {
			append intro ", $pname"
		      }
		    }
		    cgi_puts [font face=Helvetica size=+2 $intro]
		    cgi_p
		    
		    if {[info exists env(REMOTE_USER)]} {
		      set user $env(REMOTE_USER)
		    } else {
		      set user UNKNOWN
		    }

		    cgi_puts [font size=+1 face=Helvetica "Your UW NetID is [cgi_bold $user]"]

		    cgi_p
		  }
		}

		if {[file executable /usr/local/bin/wpisok]
		    && [regexp -nocase {\.[a-z]+\.[a-z]+$} [info hostname] dn]
		    && [catch {exec /usr/local/bin/wpisok deskmail${dn}}]} {
		  cgi_table_row {
		    cgi_table_data align=center colspan=2 {
		      cgi_p
		      cgi_table border=2 align=center width=90% {
			cgi_table_row {
			  cgi_table_data {
			    cgi_puts [font face=Helvetica "Unfortunately, your UW NetID does not have access rights to this server."]
			    cgi_p

			    set help_a "http://www.washington.edu/computing/uwnetid/"
			    set help_n "UW NetID Info page"
			    switch -regexp [string tolower $dn] {
			      {^\.myuw\.net$} { set ah ".Washington.edu" ; set help_a "http://www.myuw.net" ; set help_n "MyUW Homepage"}
			      {^\.washington\.edu$} { set ah ".MyUW.net"}
			      default {}
			    }

			    if {![info exists ah] || [catch {exec /usr/local/bin/wpisok deskmail${ah}}]} {
			      set blurb "Check out the [cgi_url $help_n $help_a] to find out more about email access and other NetID accessible services."
			    } else {
			      set blurb "Perhaps you intended to connect to [cgi_url WebPine$ah "https://webpine[string tolower $ah]/"] where your NetID does appear to have access rights."
			    }

			    cgi_puts [font face=Helvetica $blurb]
			  }
			}
		      }
		    }
		  }
		} else {
		  cgi_table_row {
		    cgi_table_data align=center colspan=2 {
		      cgi_javascript {
			cgi_puts "document.write('<input name=\"hPx\" value='+getDisplayHeight()+' type=hidden notab>');"
		      }
		      cgi_text User=$user type=hidden notab
		      cgi_text Pass= type=hidden notab
		      cgi_text Server=0 type=hidden notab
		      cgi_text "button=Open ${user}'s Inbox Now" type=submit
		      cgi_text lite=1 type=hidden notab

		      cgi_br
		      cgi_br
		    }
		  }

		  set offer_ssl [expr {[info exists _wp(plainservpath)] && [string length $_wp(plainservpath)]}]
		  set offer_oldver [expr {[info exists _wp(v1dir)] && [file isdirectory [file join $_wp(htdocpath) $_wp(v1dir)]]}]

		  cgi_table_row {
		    if {$offer_ssl || $offer_oldver} {
		      cgi_table_data align=center {
			cgi_table border=0 cellpadding=3 {
			  cgi_table_row {
			    cgi_table_data colspan=2 {
			      cgi_put [cgi_font size=-1 [cgi_bold "WebPine Session Options:"]]
			    }
			  }

			  if {$offer_ssl} {
			    cgi_table_row {
			      cgi_table_data valign=top {
				if {$ssl} {
				  set checked checked
				} else {
				  set checked ""
				}

				eval cgi_checkbox ssl=1 $checked
			      }
			      cgi_table_data {
				if {[info exists _wp(v1dir)]} {
				  cgi_put [cgi_font size=-1 face=Helvetica "Use [cgi_url "SSL Session Encryption" "$_wp(serverpath)/$_wp(v1dir)/wp-help/secure.html" target=_blank]"]
				} else {
				  cgi_put [cgi_font size=-1 face=Helvetica "Use SSL Session Encryption"]
				}

				cgi_br

				set t "Session encryption prevents eavesdropping, but may slow WebPine over low-speed connections. Passwords are always encrypted."
				cgi_division "style=background-color:#eeeeee" {
				  cgi_put [cgi_font size=-2 face=Helvetica $t]
				}
			      }
			    }
			  }

			  if {$offer_oldver} {
			    set nojs 1
			    cgi_table_row {
			      cgi_table_data valign=top {
				cgi_checkbox oldver=1
			      }
			      cgi_table_data {
				cgi_put [cgi_font size=-1 face=Helvetica "Use oldest version of WebPine"]
			      }
			    }
			  }
			}
		      }
		    }

		    cgi_table_data align=center valign=middle {
		      set use_query "/usr/local/bin/dmq"
		      if {[file executable $use_query]
			  && [catch {exec $use_query -w100 -g UW -u $env(REMOTE_USER) -i /images/dot2.gif} use_table] == 0} {
			cgi_puts $use_table
		      }
		    }
		  }
		}

		if {[catch {open $_wp(htdocpath)/$_wp(motd) r} id] == 0} {
		  set localtext [read $id]
		  close $id
		  cgi_table_row {
		    cgi_table_data align=center {
		      cgi_table border=0 width=90% "style=\"padding: 8\"" {
			cgi_table_row {
			  cgi_table_data bgcolor=#eeeeee {
			    cgi_puts [font size=-1 face=Helvetica $localtext]
			  }
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }

	  cgi_table_row {
	    cgi_table_data align="center" {
	      cgi_br
	      if {0} {
		cgi_puts [font size=-1 face=Helvetica [cgi_url "WebPine Frequently Asked Questions" "http://www.washington.edu/computing/faqs/webpine.html" target=_blank]]
		cgi_br

		if {[info exists _wp(v1dir)]} {
		  cgi_puts [font size=-1 face=Helvetica [cgi_url "New Version Highlights" $_wp(serverpath)/$_wp(v1dir)/wp-help/newrelease.html target=_blank]]

		  cgi_p
		}
	      }

	      set blurb "Protect your privacy![cgi_nl][cgi_url "Completely exit your browser when you finish." "http://www.washington.edu/computing/web/logout.html" target=_blank]."
	      cgi_puts [font size=-1 face=Helvetica "[cgi_bold "Warning:"] $blurb"]
	    }
	  }

	}
      }
    }
  }
}

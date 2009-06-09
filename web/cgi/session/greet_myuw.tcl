#!./tclsh

# ========================================================================
# Copyright 2006 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

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
      WPStdHtmlHdr Logon
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

    cgi_body {
      cgi_table height=100% width=85% align=center border=0 {
	cgi_table_row height=4% {
	  cgi_table_data colspan=3 {
	    cgi_put [cgi_img [WPimg dot2] border=0]
	  }
	}

	cgi_table_row {
	  cgi_table_data align=middle width="60%" {

	    cgi_put [cgi_imglink logo]
	    cgi_p

	    set intro "Welcome to Web Alpine"
	    if {[info exists env(REMOTE_USER)]} {
	      set conf [subst [lindex [lindex $_wp(hosts) 0] 2]]
	      set pldap [file join $_wp(bin) $_wp(pldap)]
	      if {[catch {exec $pldap -p $conf -u $env(REMOTE_USER)} pname] == 0 && [string length $pname]} {
		append intro ", $pname"
	      }
	    }
	    cgi_puts [font face=Helvetica size=+1 "$intro"]
	    cgi_p
	    
	    if {[info exists env(REMOTE_USER)]} {
	      set user $env(REMOTE_USER)
	    } else {
	      set user "UNKNOWN"
	    }

	    cgi_puts [font face=Helvetica "Your UW NetID is \"[cgi_bold $user]\""]
	    cgi_p
	    if {[file executable /usr/local/bin/wpisok]
		&& [regexp -nocase {\.[a-z]+\.[a-z]+$} [info hostname] dn]
		&& [catch {exec /usr/local/bin/wpisok deskmail${dn}}]} {
	      cgi_p
	      cgi_table border=0 align=center width=90% {
		cgi_table_row {
		  cgi_table_data {
		    cgi_puts [font face=Helvetica "Unfortunately, your UW NetID does not have access rights to this server."]
		    cgi_p

		    set help_a "http://www.washington.edu/computing/uwnetid/"
		    set help_n "UW NetID Info page"
		    switch -regexp [string tolower $dn] {
		      {^\.myuw\.net$} { set ah ".Washington.edu" ; set help_a "http://www.myuw.net" ; set help_n "MyUW Homepage"}
		      {^\.washington\.edu$} {set ah ".MyUW.net"}
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
	    } else {

	      cgi_form session/init method=post name=Login {
		cgi_javascript {
		  cgi_puts "document.write('<input name=\"hPx\" value='+getDisplayHeight()+' type=hidden notab>');"
		}
		cgi_text User=$user type=hidden notab
		cgi_text Pass= type=hidden notab
		cgi_text Server=0 type=hidden notab
		cgi_text "button=Open ${user}'s Inbox Now" type=submit

		cgi_br
		cgi_br
		cgi_table border=0 cellpadding=6 width=80% {
		  if {[info exists _wp(v1dir)] && [file isdirectory [file join $_wp(cgipath) $_wp(v1dir)]]} {
		    set nojs 1
		    cgi_table_row {
		      cgi_table_data valign=top {
			cgi_checkbox oldver=1
		      }
		      cgi_table_data {
			cgi_put [cgi_font size=-1 face=Helvetica "Use old version of WebPine"]
		      }
		    }
		  }

		  if {[info exists _wp(plainservpath)] && [string length $_wp(plainservpath)]} {
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
			#cgi_put [cgi_font size=-1 face=Helvetica "Use SSL Session Encryption"]

			cgi_put [cgi_font size=-1 face=Helvetica "Use [cgi_url "SSL Session Encryption" "$_wp(serverpath)/$_wp(appdir)/wp-help/secure.html" target=_blank]"]
			cgi_br
			#cgi_put [cgi_font size=-2 face=Helvetica "(Session Encryption provides confidential, secure pages, but reduces efficiency and increases page load time)"]
			if {0} {
			  set t "Performance over slow connections, such as a modem, may be improved by "
			  if {$ssl} {
			    append t "unchecking this box"
			  } else {
			    append t "leaving this box unchecked"
			  }

			  append t ".  Except for passwords, however, the session will [cgi_bold not] be secure from eavesdropping."
			} else {
			  set t "Session encryption over low-speed connections may slow WebPine, but prevents eavesdropping. Passwords are safely encrypted though."
			}

			cgi_division style=background-color:#eeeeee {
			  cgi_put [cgi_font size=-2 face=Helvetica $t]
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }

	  cgi_table_data width=2 bgcolor="#006400" {
	    cgi_puts [cgi_imglink dot]
	  }

	  cgi_table_data width="40%" {
	    cgi_table width=90% cellpadding=8 align=center {
	      cgi_table_row {
		cgi_table_data {
		  cgi_puts [font size=-1 face=Helvetica "WebPine Information:"]
		  cgi_bullet_list {
		    #cgi_li [cgi_url [font size=-1 face=Helvetica "Secure Access"] "$_wp(serverpath)/$_wp(appdir)/wp-help/secure.html" target=_blank]
		    #cgi_li [cgi_url [font size=-1 face=Helvetica "Supported Browsers"] "$_wp(serverpath)/$_wp(appdir)/wp-help/support.html" target=_blank]
		    #cgi_li [cgi_url [font size=-1 face=Helvetica "Known Problems and Limitations"] "$_wp(serverpath)/$_wp(appdir)/wp-help/issues.html" target=_blank]
		    cgi_li [cgi_url [font size=-1 face=Helvetica "Frequently Asked Questions"] "http://www.washington.edu/computing/faqs/webpine.html" target=_blank]
		  }
		}
	      }

	      if {[catch {open [file join $_wp(cgipath) $_wp(motd)] r} id] == 0} {
		set localtext [read $id]
		close $id
		cgi_table_row {
		  cgi_table_data {
		    cgi_puts [font size=-1 face=Helvetica $localtext]
		  }
		}
	      }

	      set use_query "/usr/local/bin/dmq"
	      if {[file executable $use_query]
		  && [catch {exec $use_query -w100 -g MyUW -u $env(REMOTE_USER) -i /images/dot2.gif} use_table] == 0} {
		cgi_table_row {
		  cgi_table_data {
                    cgi_puts $use_table
		  }
		}
	      }
	    }
	  }
	}

	cgi_table_row {
	  cgi_table_data colspan=3 height=12% {
	    cgi_center {
	      set blurb "Protect your privacy![cgi_nl][cgi_url "Completely exit your Web browser when you finish." "http://www.washington.edu/computing/web/logout.html" target=_blank]."
	      cgi_puts [font size=-1 face=Helvetica "[cgi_bold "Warning:"] $blurb"]
	    }
	  }
	}
      }
    }
  }
}

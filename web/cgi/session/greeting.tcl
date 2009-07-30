#!./tclsh
# $Id: greeting.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# suck in any global config
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

      if {$_wp(flexserver)} {
	cgi_javascript {
	  cgi_put  "function bogus(f,o) {"
	  cgi_put  " if(o.value.length == 0){"
	  cgi_put  "  alert('What '+f+' will you be using to log in to your IMAP server?\\nPlease fill in the '+f+' field.');"
	  cgi_put  "  o.focus();"
	  cgi_put  "  return true;"
	  cgi_put  " }"
	  cgi_put  " return false;"
	  cgi_puts "}"
	  cgi_put  "function doSubmit() {"
	  cgi_put  " var f = document.Login;"
	  cgi_put  " var l = f.Server;"
	  cgi_put  " var o = l.options\[l.selectedIndex\];"
	  cgi_put  " if(bogus('Username',f.User) || bogus('Password',f.Pass)) return false;"
	  cgi_put  " if(o.value < 0){"
	  cgi_put  "  o.value = alien_server;"
	  cgi_put  " }"
	  cgi_put  " return true;"
	  cgi_puts "}"
	  cgi_put  "function customServer() {"
	  cgi_put  " var f = document.Login;"
	  cgi_put  " var l = f.Server;"
	  cgi_put  " var o = l.options\[l.selectedIndex\];"
	  if {[info exists env(REMOTE_USER)]} {
	    cgi_put "  f.User.value = (o.value < 0) ? alien_user : def_user;"
	    cgi_put "  f.Pass.value = (o.value < 0) ? alien_pass : def_pass;"
	  }
	  cgi_put  " if(o.value < 0){"
	  cgi_put  "  if(s = prompt('IMAP Server Name (append \"/ssl\" for secure link)',o.s ? o.s : '')){"
	  cgi_put  "   alien_server = s;"
	  cgi_put  "   o.text = 'Your Server: '+alien_server;"
	  cgi_put  "   f.User.onfocus = null;"
	  cgi_put  "   f.Pass.onfocus = null;"
	  cgi_put  "   f.User.focus();"
	  cgi_put  "  }"
	  cgi_put  "  else if(alien_server.length == 0){"
	  if {[info exists env(REMOTE_USER)]} {
	    cgi_put  "   f.User.onfocus = f.User.blur;"
	    cgi_put  "   f.Pass.onfocus = f.Pass.blur;"
	    cgi_put  "   f.User.value = def_user;"
	    cgi_put  "   f.Pass.value = def_pass;"
	  }
	  cgi_put  "   l.selectedIndex = 0;"
	  cgi_put  "  }"
	  cgi_put  " }"
	  if {[info exists env(REMOTE_USER)]} {
	    cgi_put  " else {"
	    cgi_put  "   f.User.onfocus = f.User.blur;"
	    cgi_put  "   f.Pass.onfocus = f.Pass.blur;"
	    cgi_put  " }"
	  }
	  cgi_puts "}"
	  if {[info exists env(REMOTE_USER)]} {
	    cgi_put  "var def_user = new Array('$env(REMOTE_USER)');"
	    cgi_put  "var def_pass = new Array('x');"
	    cgi_put  "var alien_server = '';"
	    cgi_put  "var alien_user = '';"
	    cgi_put  "var alien_pass = '';"
	    cgi_put  "function customSave() {"
	    cgi_put  " var f = document.Login;"
	    cgi_put  " var s = f.Server.options\[f.Server.selectedIndex\];"
	    cgi_put  " if(s.value < 0){"
	    cgi_put  "  alien_user = f.User.value;"
	    cgi_put  "  alien_pass = f.Pass.value;"
	    cgi_put  " }"
	    cgi_puts "}"
	  }
	}
      }
    }

    if {[info exists env(REMOTE_USER)]} {
      set onload ""
    } else {
      set onload "onLoad=document.Login.User.focus()"
    }

    cgi_body $onload {
      if {$_wp(flexserver)} {
	set onsubmit "onSubmit=return doSubmit()"
      } else {
	set onsubmit ""
      }

      cgi_form session/init method=post enctype=multipart/form-data name=Login $onsubmit {
	cgi_javascript {
	  cgi_puts "document.write('<input name=\"hPx\" value='+window.getDisplayHeight()+' type=hidden notab>');"
	}

	cgi_table height=100% width=100% align=center {

	  cgi_table_row {
	    cgi_table_data align=center valign=bottom height=18% colspan=8 {
	      set intro "Welcome to $_wp(appname)"
	      if {[info exists env(REMOTE_USER)]} {
		set conf [subst [lindex [lindex $_wp(hosts) 0] 2]]
		set pldap [file join $_wp(bin) $_wp(pldap)]
		if {[catch {exec $pldap -p $conf -u $env(REMOTE_USER)} pname] == 0} {
		  append intro ", $pname"
		}
	      }

	      cgi_put [font size=+2 face=Helvetica [bold $intro]]
	    }
	  }

	  cgi_table_row {
	    cgi_table_data width=36% height=35% align=right valign=middle {
	      cgi_put [cgi_imglink logo]
	    }

	    cgi_table_data width=5% {
	      cgi_puts [cgi_nbspace]
	    }

	    cgi_table_data {
	      cgi_table border=0 cellspacing=8 {
		cgi_table_row {
		  cgi_table_data align=right valign=bottom {
		    cgi_puts [font size=+1 face=Helvetica "Username:"]
		  }

		  cgi_table_data align=left valign=bottom {
		    if {[info exists env(REMOTE_USER)]} {
		      set user "User=$env(REMOTE_USER)"
		      set rdonly "onFocus=this.blur();"
		    } else {
		      set user "User="
		      set rdonly ""
		    }

		    if {$_wp(flexserver) && [info exists env(REMOTE_USER)]} {
		      set onblur onBlur=customSave()
		    } else {
		      set onblur ""
		    }

		    cgi_text $user type=text maxlength=30 tableindex=1 $rdonly $onblur
		  }
		}

		cgi_table_row {
		  cgi_table_data align=right valign=middle {
		    cgi_puts [font size=+1 face=Helvetica "Password:"]
		  }

		  cgi_table_data align=left valign=middle {
		    if {[info exists env(REMOTE_USER)]} {
		      set pass "Pass=*"
		      set rdonly "onFocus=this.blur();"
		    } else {
		      set pass "Pass="
		      set rdonly ""
		    }

		    if {$_wp(flexserver) && [info exists env(REMOTE_USER)]} {
		      set onblur onBlur=customSave()
		    } else {
		      set onblur ""
		    }

		    cgi_text $pass type=password maxlength=30 tableindex=2 $rdonly $onblur
		  }
		}

		cgi_table_row {
		  cgi_table_data align=right valign=middle {
		    cgi_puts [font face=Helvetica size=+1 "Server:"]
		  }

		  cgi_table_data align=left valign=top {
		    if {[info exists _wp(hosts)]} {
		      if {$_wp(flexserver)} {
			set onchange onChange=customServer()
			if {[info exists env(REMOTE_USER)]} {
			  set onblur onBlur=customSave()
			} else {
			  set onblur ""
			}
		      } else {
			set onchange ""
			set onblur ""
		      }

		      cgi_select Server align=left $onchange $onblur {
			for {set j 0} {$j < [llength $_wp(hosts)]} {incr j} {
			  cgi_option [lindex [lindex $_wp(hosts) $j] 0] value=$j
			}

			if {$_wp(flexserver)} {
			  cgi_javascript {
			    cgi_puts "document.write('<option value=\"-1\">Server of Your Choice');"
			  }
			}
		      }
		    } else {
		      cgi_text Server= type=text maxlength=256
		    }
		  }
		}
	      }
	    }
	  }

	  if {[info exists env(REMOTE_USER)]} {
	    cgi_table_row {
	      cgi_table_data align=center valign=top height=1% colspan=8 {
		cgi_puts "Protect your privacy! When you finish, [cgi_url "completely exit your Web browser" "http://www.washington.edu/computing/web/logout.html"]."
	      }
	    }
	  }

	  cgi_table_row {
	    cgi_table_data align=center valign=top colspan=8 "style=padding-bottom:10" {
	      cgi_submit_button login=Login
	    }
	  }

	  if {[info exists _wp(plainservpath)] && [string length $_wp(plainservpath)]} {
	    cgi_table_row {
	      cgi_table_data colspan=8 align=center valign=middle {
		cgi_table border=0 width=30% {
		  cgi_table_row {
		    cgi_table_data rowspan=2 valign=top {
		      if {$ssl} {
			set checked checked
		      } else {
			set checked ""
		      }

		      cgi_checkbox ssl=1 $checked
		    }

		    cgi_table_data {
		      cgi_put [cgi_font size=-1 face=Helvetica "Use [cgi_url "SSL Session Encryption" "$_wp(serverpath)/$_wp(appdir)/$_wp(ui1dir)/help/secure.html" target=_blank]"]
		      cgi_br
		      set t "Session encryption over low-speed connections may slow WebPine, but prevents eavesdropping. Passwords are safely encrypted though."

		      cgi_division style=background-color:#eeeeee {
			cgi_put [cgi_font size=-2 face=Helvetica $t]
		      }
		    }
		  }
		}
	      }
	    }
	  } else {
	    cgi_text ssl=1 type=hidden notab
	  }

	  if {[info exists _wp(oldserverpath)] && [regexp {^[Hh][Tt][Tt][Pp][Ss]://} $_wp(oldserverpath)]} {
	    cgi_table_row {
	      cgi_table_data colspan=8 align=center valign=middle {
		cgi_division "style=\"width: 30% ; font-family: Helvetica; font-size: small ; background-color: #EEEEEE; padding: 8 2 \"" {
		  cgi_puts "For the time being, the [cgi_url "old version" $_wp(oldserverpath)] is still available."
		}
	      }
	    }
	  }

	  if {[catch {open [file join $_wp(cgipath) $_wp(motd)] r} id] == 0} {
	    cgi_table_row {
	      cgi_table_data colspan=8 height=20% valign=top {
		cgi_table width=100% {
		  cgi_table_data height=40 width=18% {
		    # cgi_puts [cgi_imglink bang]
		    cgi_puts [cgi_nbspace]
		  }

		  cgi_table_data {
		    cgi_puts [font size=-1 face=Helvetica [read $id]]
		  }

		  cgi_table_data width=18% {
		    # cgi_puts [cgi_imglink bang]
		    cgi_puts [cgi_nbspace]
		  }
		}
	      }
	    }

	    close $id
	  }
	}
      }
    }
  }
}

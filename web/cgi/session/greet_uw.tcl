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
      WPStdHtmlHdr "WebPine - Logon"
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

      cgi_puts  "<style type='text/css'>"
      cgi_puts  "BODY { background-color: #2D0D55; font-family: Arial, sans-serif }"
      cgi_puts "</style>"
    }

    cgi_body {
      cgi_puts [cgi_nbspace][cgi_nl]
      cgi_form session/init method=post name=Login {
	cgi_table width=90% background=[WPimg Lavender_Chiffon] align=center border=0 "style=\"border: 1px black solid\;\"" {
	  cgi_table_row {
	    cgi_table_data xalign=center "style=\"padding-left: 10% ; border-bottom: 2px solid goldenrod\"" {
	      cgi_table cellspacing=0 cellpadding=0 border=0 {
		cgi_table_row {
		  cgi_table_data align=center {
		    cgi_puts [cgi_imglink logo]
		  }

		  cgi_table_data align=center width=15% {
		    cgi_put [cgi_img [WPimg dot2] height=120 width=1]
		  }

		  cgi_table_data nowrap {
		    cgi_division "style=\"background-color: #DEDEF7; border: 1px solid goldenrod\"" {
		      cgi_division "style=\"color: black; xfont-weight: bold; font-size: 18pt; padding: 10 24 \"" {
			cgi_puts "Welcome to WebPine"
		      }
		      cgi_division align=right {
			cgi_puts [cgi_span "style=background-color: goldenrod; color: white; padding: 1 4 ; font-size: smaller" "Updated April 2006"]
		      }
		    }
		  }
		}
	      }
	    }
	  }

	  cgi_table_row {
	    cgi_table_data {
	      cgi_table border=0 cellspacing=15 cellpadding=1 width=100% bgcolor=white "style=\"border: 1px solid goldenrod\"" {
		cgi_table_row {
		  cgi_table_data align=center colspan=2 {
		    
		    if {[info exists env(REMOTE_USER)]} {
		      set user $env(REMOTE_USER)
		    } else {
		      set user UNKNOWN
		    }

		    cgi_puts [cgi_span "style=font-size: larger" "Your UW NetID is [cgi_bold $user]"]

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
		      if {[string length $user] && [string compare UNKNOWN $user]} {
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
		      } else {
			cgi_division "style=\"width: 66%; border: 1px solid red; background-color: pink\"" {
			  cgi_puts "You will be unable to log in until WebPine is presented with a valid UW NetID."
			}
		      }
		    }
		  }

		  cgi_table_row {
		    if {[info exists _wp(plainservpath)] && [string length $_wp(plainservpath)]} {
		      cgi_table_data align=center width=50% {
			cgi_table border=0 cellpadding=3 {
			  cgi_table_row {
			    cgi_table_data colspan=2 "style=\"font: helvetica ; font-size: smaller; font-weight: bold\"" {
			      cgi_put "WebPine Session Options:"
			    }
			  }

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
			      cgi_put [cgi_font size=-1 face=Helvetica "Use [cgi_url "SSL Session Encryption" "$_wp(serverpath)/$_wp(appdir)/help/secure.html" target=_blank]"]
			      cgi_br

			      set t "Session encryption prevents eavesdropping, but may slow WebPine over low-speed connections. Passwords are always encrypted."
			      cgi_division "style=background-color:#eeeeee" {
				cgi_put [cgi_font size=-2 face=Helvetica $t]
			      }
			    }
			  }
			}
		      }
		    }

		    cgi_table_data align=center valign=middle {
		      cgi_table border=0 cellpadding=3 {
			cgi_table_row {
			  cgi_table_data "style=\"font: helvetica ; font-size: smaller; font-weight: bold ; text-align: center\"" {
			    cgi_put "Please check out our related email services such as [cgi_url "spam" "https://mailsrvc-h.u.washington.edu/edm/filter.cgi/spam" target=_blank] and "
			    cgi_put "[cgi_url "message filtering" "https://mailsrvc-h.u.washington.edu/edm/filter.cgi/" target=_blank], "
			    cgi_put "and [cgi_url "vacation service" "https://mailsrvc-h.u.washington.edu/edm/filter.cgi/vacation" target=_blank], all of which "
			    cgi_put "are part of our [cgi_url "Email Delivery Manager" "https://mailsrvc-h.u.washington.edu/edm/filter.cgi/" target=_blank]."
			  }
			}
			cgi_table_row {
			  cgi_table_data {
			    cgi_puts {
<table align="center" bgcolor="#ccccef" border="0" cellpadding="5" cellspacing="0" width="100%"
>
 <tbody><tr><td colspan="3" align="center">
  <font size="-1">
    <strong>
        Your UW Email and File Storage<br>
        on <font color="blue">Apr 5, 2006</font>
        at <font color="blue">01:38 pm</font>
    </strong>
  </font>
 </td></tr>
 <tr><td align="right"><small>0%</small></td><td width="60%">
  <table align="center" border="0" cellpadding="0" cellspacing="0" width="100%">
   <tbody><tr>
    <td style="border-style: solid; border-color: black; border-width: 1px 0px 1px 1px; backgro
und-color: rgb(64, 128, 64);" width="14%">
     <img src="greeting.tcl_files/dot2.gif" height="1" width="1">
    </td>
    <td style="border: 1px solid black; background-color: rgb(255, 255, 255);" width="86%">
     <img src="greeting.tcl_files/dot2.gif" height="1" width="1">&nbsp;
    </td>
   </tr>
  </tbody></table>
 </td><td align="left"><small>100%</small></td></tr>
 <tr><td colspan="3">
   <font size="-1">
     <b>NOTE:</b> Recent changes may not appear here.
   </font>
 </td></tr>
 <tr><td colspan="3" align="center">
    <font size="+1">
      <a href="https://uwnetid.washington.edu/disk/"><b>View&nbsp;Current&nbsp;Usage</b></a>
 </font>
 </td></tr>
</tbody></table>
			    }

			    set use_query "/usr/local/bin/dmq"
			    if {[file executable $use_query]
				&& [catch {exec $use_query -w100 -g UW -u $env(REMOTE_USER) -i /images/dot2.gif} use_table] == 0} {
			      cgi_division "style=\"align: center; margin: 0 10%\"" {
				cgi_puts $use_table
			      }
			    }
			  }
			}
		      }
		    }
		  }
		}

		if {[catch {open [file join $_wp(cgipath) $_wp(motd)] r} id] == 0} {
		  set localtext [read $id]
		  close $id
		  cgi_table_row {
		    cgi_table_data colspan=2 align=center {
		      cgi_division "style=\"margin: 0 20% ; background-color: \#eeeeee; font-style: Helvetica ; font-size: small\"" {
			cgi_puts $localtext
		      }
		    }
		  }
		}
	      }
	    }
	  }

	  cgi_table_row {
	    cgi_table_data align="center" "style=\"border-top: 2px solid goldenrod\"" {
	      if {[info exists _wp(faq)] && [regexp {^[Hh][Tt][Tt][Pp][Ss]?://} $_wp(faq)]} {
		cgi_br
		cgi_puts [font size=-1 face=Helvetica [cgi_url "WebPine Frequently Asked Questions" $_wp(faq) target=_blank]]
	      }

	      if {[info exists _wp(releaseblurb)] && [regexp {^[Hh][Tt][Tt][Pp][Ss]?://} $_wp(releaseblurb)]} {
		cgi_br
		cgi_puts [font size=-1 face=Helvetica [cgi_url "New Version Highlights" $_wp(releaseblurb) target=_blank]]
	      }

	      if {[info exists _wp(oldserverpath)] && [regexp {^[Hh][Tt][Tt][Pp][Ss]?://} $_wp(oldserverpath)]} {
		cgi_br
		cgi_puts [font size=-1 face=Helvetica "(The [cgi_url "old version" $_wp(oldserverpath)] is available for a limited time.)"]
	      }

	      cgi_p

	      set blurb "Protect your privacy![cgi_nl][cgi_url "Completely exit your browser when you finish." "http://www.washington.edu/computing/web/logout.html" target=_blank]."
	      cgi_puts [font size=-1 face=Helvetica "[cgi_bold "Warning:"] $blurb"]
	    }
	  }

	}
      }
    }
  }
}

# Web Alpine Config options
# $Id: alpine.tcl 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $
# ========================================================================
# Copyright 2006-2008 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

encoding system "utf-8"

set _wp(appname)	Alpine
set _wp(admin)		admin@sample-domain.edu
set _wp(helpdesk)	admin@sample-domain.edu
set _wp(comments)	help@sample-domain.edu

# List of userid's allowed to request the monitor script output
set _wp(monitors)	{}

# directory prefix web server uses for web alpine page requests
# Note: set to {} if DocumentRoot set to the root of web alpine cgi scripts
set _wp(urlprefix)	webmail

# file system path to CGI application files
# directory containing web alpine application scripts and supporting tools
set _wp(fileroot)	/usr/local/libexec/alpine

set _wp(tmpdir)		/tmp

# NOTE: Make SURE tclsh and alpine.tcl symlinks in this directory
set _wp(cgipath)	[file join $_wp(fileroot) cgi]

# CGI scripts implementing U/I, session cookie scope
set _wp(appdir)		alpine

# UI versions
set _wp(ui1dir)		1.0
set _wp(ui2dir)		2.0

# place for CGI scripts not requiring session-key
set _wp(pubdir)		pub

# place for binaries referenced by the CGI scripts
set _wp(bin)		[file join $_wp(fileroot) bin]
set _wp(servlet)	alpined
set _wp(pc_servlet)	pc_alpined
set _wp(pldap)          alpineldap

# place for config files referenced by the CGI scripts
set _wp(confdir)	[file join $_wp(fileroot) config]
set _wp(conffile)	pine.conf
set _wp(defconf)	$_wp(conffile)

# place for library files used by CGI scripts
set _wp(lib)		[file join $_wp(fileroot) lib]

# directory used temporarily to stage attatched and detached files
set _wp(detachpath)	[file join $_wp(fileroot) detach]

set _wp(imagepath)	[file join / $_wp(urlprefix) images]

set _wp(buttonpath)	[file join $_wp(imagepath) buttons silver]

set _wp(staticondir)	env

set _wp(servername)	[info hostname]

# MUST specify SSL/TLS connection
set _wp(serverport)	{}
set _wp(serverpath)	https://[file join [join [eval list $_wp(servername) $_wp(serverport)] :] $_wp(urlprefix)]

# MAY specify a plaintext connection (comment out if plain support undesired)
set _wp(plainport)	{}
set _wp(plainservpath)	http://[file join [join [eval list $_wp(servername) $_wp(plainport)] :] $_wp(urlprefix)]

# url of faq page(s) available from initial greeting page
#set _wp(faq)		"http://www.yourserver/faqs/alpine.html"

# url of informational page accessible from initial greeting page
set _wp(releaseblurb)	"$_wp(plainservpath)/alpine/help/release.html"

# url of previous version server to be accessible from initial greeting page
#set _wp(oldserverpath)	"https://previous.version.server.edu:444/"

# session id length: make sure the integer count below matches what's built
# into the pubcookie: "src/pubcookie/wp_uidmapper_lib.h:#define WP_KEY_LEN 6"
set _wp(sessidlen)	6

# Where and what format the alpined comm socket should take
set _wp(sockdir)	$_wp(tmpdir)
set _wp(sockpat)	wp%s

# skin settings
set _wp(bordercolor)	#FEFAC9
set _wp(menucolor)	#3E2E6D
set _wp(dialogcolor)	#FEFAC9
set _wp(titlecolor)	#000000
set _wp(logodir)	alpine

# various timerouts, dimensions and feature settings
set _wp(refresh)	600
set _wp(timeout)	900
set _wp(autodraft)	300
set _wp(logoutpause)	60
set _wp(indexlines)	20
set _wp(indexlinesmax)	50
set _wp(indexheight)	24
set _wp(navheight)	28
set _wp(width)		80
set _wp(titleheight)	34
set _wp(titlesep)	4
set _wp(config)		remote_pinerc
set _wp(motd)		motd
set _wp(save_cache_max)	6
set _wp(fldr_cache_max)	20
set _wp(fldr_cache_def)	3
set _wp(statushelp)	0
set _wp(imgbuttons)	0
set _wp(keybindings)	1
set _wp(dictionary)	0
set _wp(debug)		0
set _wp(cmdtime)	0
set _wp(evaltime)	0
set _wp(menuargs)	{width="112" nowrap valign=top}
set _wp(ispell)		/usr/local/bin/ispell

# Yahoo! User Interface Library location
#set _wp(yui)		$_wp(serverpath)/$_wp(appdir)/$_wp(ui2dir)/lib/yui
set _wp(yui)		"http://yui.yahooapis.com/2.7.0"

# usage reporter - input: username as first command line argument
#                  output: space separated integers usage and total
#set _wp(usage)		$_wp(bin)/usage.tcl
#set _wp(usage_link)	"https://uwnetid.washington.edu/disk/"

# limit uploads to 1 file at a time, maximum 20MB.
set _wp(uplim_files)	1
set _wp(uplim_bytes)	20000000

# verify sessid from consistent REMOTE_ADDR (set to 0 for proxying clusters)
set _wp(hostcheck)	0

# set to list of domains for which ssl is NOT required
#set _wp(ssl_safe_domains)	{}

# set to list of address blocks or ranges for which ssl is NOT required
#set _wp(ssl_safe_addrs)		{}

# set this value to zero to turn OFF ssl by default
set _wp(ssl_default)	1

# allow connecting user to specify imap server on greeting page
set	_wp(flexserver)	1

# make sure tmp files and such are ours alone to read/write
catch {exec umask 044}

#fix up indexheight so it isn't too high or too low
set _wp(indexheight) [expr {$_wp(indexheight) <= 20 ? 20 : $_wp(indexheight) >= 30 ? 30 : $_wp(indexheight)}]

# SPAM reporting facility, if set "Report Spam" button appears at top of View Page
#set _wp(spamaddr)	spamaddr@sample-domain.edu
#set _wp(spamfolder)	junk-mail
#set _wp(spamsubj)	"ATTACHED SPAM"

# external mail filter config link
#set _wp(filter_link)	http://delivery-filter.sample-domain.edu/filter/config

# external vacation config link
#set _wp(vacation_link)	http://vacation.sample-domain.edu/vacation/config

#
# Nickname server bindings.  If not present, prompt for the
# destination of the default pinerc location.
#
set _wp(hosts) {
    {
      Deskmail
      $User.deskmail.washington.edu/ssl
      $_wp(confdir)/conf.deskmail
    }
}

# Everybody inherits the cgi, comm packages
lappend auto_path $_wp(lib)

package require cgi
package require WPComm

# Recipient of bad news bubbling up from cgi.tcl...
cgi_admin_mail_addr $_wp(admin)

cgi_sendmail {}

#cgi_mail_relay localhost
cgi_mail_relay smtpserver.sample-domain.edu

# set permissions for owner-only handling
cgi_tmpfile_permissions 0640

#set upload limits
cgi_file_limit $_wp(uplim_files) $_wp(uplim_bytes)

# universal body tag parameters
cgi_body_args link=#0000FF vlink=#000080 alink=#FF0000 marginwidth=0 marginheight=0 topmargin=0 leftmargin=0

# Common Images Image definitions
cgi_imglink logo		[file join $_wp(imagepath) logo $_wp(logodir) big.gif]	border=0	"alt=Web Alpine"
cgi_imglink smalllogo		[file join $_wp(imagepath) logo $_wp(logodir) small.gif]	border=0	"alt=About Web Alpine"
cgi_imglink background		[file join $_wp(imagepath) logo $_wp(logodir) back.gif]		border=0	align=top
cgi_imglink dot			[file join $_wp(imagepath) dot2.gif]		border=0	align=top
cgi_imglink increas		[file join $_wp(imagepath) increas4.gif]	border=0	align=absmiddle
cgi_imglink decreas		[file join $_wp(imagepath) decreas4.gif]	border=0	align=absmiddle
cgi_imglink expand		[file join $_wp(imagepath) b_plus.gif]	border=0	"alt=Expand"	height=9 width=9
cgi_imglink contract		[file join $_wp(imagepath) b_minus.gif]	border=0	"alt=Collapse"	height=9 width=9
cgi_imglink fullhdr		[file join $_wp(imagepath) hdr.gif]		border=0	"alt=Full Header"
cgi_imglink nofullhdr		[file join $_wp(imagepath) hdrnon.gif]	border=0	"alt=Digested Header"
cgi_imglink bang		[file join $_wp(imagepath) caution.gif]	border=0	"alt=!"
cgi_imglink postmark		[file join $_wp(imagepath) postmark.gif]	border=0	"alt=New Mail"
cgi_imglink gtab		[file join $_wp(imagepath) tabs gtab.gif]	border=0	align=top
cgi_imglink gdtab		[file join $_wp(imagepath) tabs gdtab.gif]	border=0	align=top
cgi_imglink abtab		[file join $_wp(imagepath) tabs abtab.gif]	border=0	align=top
cgi_imglink abdtab		[file join $_wp(imagepath) tabs abdtab.gif]	border=0	align=top
cgi_imglink ctab		[file join $_wp(imagepath) tabs ctab.gif]	border=0	align=top
cgi_imglink cdtab		[file join $_wp(imagepath) tabs cdtab.gif]	border=0	align=top
cgi_imglink ftab		[file join $_wp(imagepath) tabs ftab.gif]	border=0	align=top
cgi_imglink fdtab		[file join $_wp(imagepath) tabs fdtab.gif]	border=0	align=top
cgi_imglink mltab		[file join $_wp(imagepath) tabs mltab.gif]	border=0	align=top
cgi_imglink mldtab		[file join $_wp(imagepath) tabs mldtab.gif]	border=0	align=top
cgi_imglink mvtab		[file join $_wp(imagepath) tabs mvtab.gif]	border=0	align=top
cgi_imglink mvdtab		[file join $_wp(imagepath) tabs mvdtab.gif]	border=0	align=top
cgi_imglink rtab		[file join $_wp(imagepath) tabs rtab.gif]	border=0	align=top
cgi_imglink rdtab		[file join $_wp(imagepath) tabs rdtab.gif]	border=0	align=top


# Link definitions
cgi_link Admin "Web Alpine Administrator" "mailto:$_wp(admin)"
cgi_link Start "Web Alpine Home Page" "$_wp(serverpath)/session/greeting.tcl" target=_top

# Internally referenced CGI directory root
cgi_root $_wp(serverpath)
cgi_suffix .tcl

# have cgi.tcl convert eols in muiltipart/form-data
set _cgi(no_binary_upload) 1

proc WPSocketName {sessid} {
  global _wp

  return [file join $_wp(sockdir) [format $_wp(sockpat) $sessid]]
}

proc WPValidId {{sessid {}}} {
    global _wp env

    if {[string length $sessid] == 0} {
      set created 1

      # Session Handle: a bit reasonably random number.  the format
      # is convenient for pubcookie auth'd support
      set rnum {}
      set rsrc /dev/urandom
      set idbytelength [expr {$_wp(sessidlen) * 4}]
      if {[file readable $rsrc] && [catch {open $rsrc r} fp] == 0} {
	while {1} {
	  for {set i 0} {$i < $idbytelength} {incr i} {
	    if {$i && ($i % 4) == 0} {
	      append rnum "."
	    }

	    if {[catch {read $fp 1} n] == 0} {
	      binary scan $n c x
	      set x [expr ($x & 0xff)]
	      append rnum [format {%02x} $x]
	    } else {
	      set rnum {}
	      break
	    }
	  }

	  if {[file exists [WPSocketName $rnum]]} {
	    set rnum {}
	  } else {
	    break
	  }
	}

	close $fp
      }

      # second choice for random numbers
      if {[string length $rnum] == 0} {
	expr srand([clock seconds])
	for {set i 0} {$i < $idbytelength} {incr i 4} {
	  if {$i && ($i % 4) == 0} {
	    append rnum "."
	  }

	  append rnum [format {%08x} [expr int((100000000 * rand()))]]
	}
      }

      # generate a session ID
      set _wp(sessid) $rnum
    } else {
      set sessidparts [split $sessid {@}]
      switch [llength $sessidparts] {
	1 {
	  set _wp(sessid) $sessid
	}
	2 {
	  if {[string compare [string tolower [lindex $sessidparts 1]] [string tolower [info hostname]]]} {
	    regexp {^([a-zA-Z]*://).*} [cgi_root] match proto
	    error [list redirect "${proto}[lindex $sessidparts 1]:$env(SERVER_PORT)?$env(QUERY_STRING)"]
	  } else {
	    set _wp(sessid) [lindex $sessidparts 0]
	  }
	}
	default {
	  error "Malformed Session ID: $sessid"
	}
      }
    }

    set _wp(sockname) [WPSocketName $_wp(sessid)]

    if {[info exists _wp(cumulative)]} {
      rename WPCmd WPCmd.orig
      rename WPCmdTimed WPCmd
    }

    if {[info exists _wp(hostcheck)] && $_wp(hostcheck) == 1 && ![info exists created]
	&& [catch {WPCmd set wp_client} client] == 0
	&& (([info exists env(REMOTE_ADDR)] && [string length $env(REMOTE_ADDR)] && [string compare $client $env(REMOTE_ADDR)])
	    || ([info exists env(REMOTE_HOST)] && [string length $env(REMOTE_HOST)] && [string compare $client $env(REMOTE_HOST)]))} {
      error "Request from unrecognized client"
    }
}

proc WPAbort {} {
    WPCleanup
    cgi_exit
}

proc WPCleanup {} {
    global _wp

    if {[info exists _wp(cleanup)]} {
	foreach item $_wp(cleanup) {
	    catch {eval $item}
	}
    }
}

proc WPEval {vars cmd} {
    global _wp

    if {$_wp(cmdtime) || $_wp(evaltime)} {
	set _wp(cumulative) 0
    }

    set _wp(cmd) $cmd
    set _wp(vars) [linsert $vars 0 [list sessid "Missing Session ID"]]

    uplevel 1 {
      cgi_eval {
	if {$_wp(debug) > 1} {
	    cgi_debug -on
	}

	# Session id?
	if {[catch {WPGetInputAndID sessid}]} {
	  return
	}

	foreach item $_wp(vars) {
	  if {[catch {eval WPImport $item} errstr]} {
	    WPInfoPage "Web Alpine Error" [font size=+2 $errstr] "Please close this window."
	    return
	  }
	}

	# evaluate the given script
	if {[catch {cgi_buffer $_wp(cmd)} result]} {

	    reset_cgi_state

	    if {[string index $result 0] == "_"} {
	      switch -- [lindex $result 0] {
		_info {
		  WPInfoPage [lindex $result 1] [font size=+2 [lindex $result 2]] [lindex $result 3]
		}
		_action {
		  switch -regexp -- [lindex $result 2] {
		    "[Ii]nactive [Ss]ession" {
		      WPInactivePage
		    }
		    default {
		      if {[string length [lindex $result 3]]} {
			set remedy [lindex $result 3]
		      } else {
			set remedy "   Click your browser's Back button to return to previous page."
		      }
		      WPInfoPage "[lindex $result 1] Error" [font size=+2 [lindex $result 2]] \
			  "Please report this to the [cgi_link Admin].$remedy"
		    }
		  }
		}
		_redirect {
		  cgi_http_head {
		    cgi_redirect [lindex $result 1]
		  }

		  cgi_html { cgi_body {} }
		}
		_close {
		  if {[string length [lindex $result 1]] == 0} {
		    set result "Indeterminate error"
		  }

		  WPInfoPage "Web Alpine Error" [font size=+2 [lindex $result 1]] "Please close this window."
		}
		default {
		  if {[string length $result]} {
		    WPInfoPage "Web Alpine Error" [font size=+2 "Eval Error: $result"] \
			"Please complain to the [cgi_link Admin].   Click Back button to return to previous page."
		  } else {
		    WPInfoPage "Web Alpine Error" [font size=+2 "Indeterminate error response"] \
			"Please complain to the [cgi_link Admin] and click Back button to return to previous page."
		  }
		}
	      }
	    } else {
	      if {[regexp {[Ii]nactive [Ss]ession} $result]} {
		WPInactivePage
	      } else {
		WPInfoPage "Web Alpine Error" [font size=+2 "Error: $result"] \
		    "Please report this to the [cgi_link Admin]. Try clicking your browser's Back button to return to a working page."
	      }
	    }
	} else {
	    catch {cgi_puts $result}
	}
    }
  }

    # cleanup here
    WPCleanup

    if {[info exists _wp(cumulative)]} {
      WPdebug "Cumulative Eval: $_wp(cumulative)"
      unset _wp(cumulative)
    }
}

proc WPGetInputAndID {_sessid} {
  global _wp
  upvar $_sessid sessid

  # Import data and validate it
  if {[catch {cgi_input "sessid=8543949466398&"} result]} {
    WPInfoPage "Web Alpine Error" [font size=+2 $result] "Please close this window."
    error "Cannot get CGI Input"
  }

  if {[catch {WPImport sessid "Missing Session ID"} errstr]} {
    if {[regexp {.*sessid.*no such.*} $errstr]} {
      WPInactivePage [list "Your browser may have failed to send the necessary <i>cookie</i> information.  Please verify your browser configuration has cookies enabled."]
    } else {
      WPInfoPage "Web Alpine Error" [font size=+2 $errstr] "Please close this window."
    }

    error "Session ID Failure"
  } else {
    # initialization here
    if {[catch {WPValidId $sessid} result]} {
      if {[string compare [lindex $result 0] redirect]} {
	WPInfoPage "Web Alpine Error" [font size=+2 "$result"] \
	    "Please complain to the [cgi_link Admin] and visit the [cgi_link Start] later."
      } else {
	cgi_http_head {
	  cgi_redirect [lindex $result 1]
	}
      }

      error "Unrecoverable Error"
    } elseif {$_wp(sessid) == 0} {
      WPInactivePage
      error "Inactive Session"
    }

    if {[catch {WPCmd set serverroot} serverroot] == 0} {
      cgi_root $serverroot
    }
  }
}

proc WPCmdEval {args} {
  return [eval $args]
}

proc WPCmd {args} {
  global _wp

  return [WPSend $_wp(sockname) $args]
}

proc WPCmdTimed {args} {
  global _wp

  set t [lindex [time {set r [WPSend $_wp(sockname) $args]}] 0]
  incr _wp(cumulative) $t

  if {$_wp(cmdtime)} {
    WPdebug "time $t : $args"
  }

  return $r
}

proc WPLoadCGIVar {_var} {
  upvar $_var var

  if {[catch {cgi_import_as $_var var} result]
      && [catch {WPCmd set $_var} var]
      && [catch {cgi_import_cookie_as $_var var} result]} {
    error [list _action "Import Cookie $_var" $result]
  }
}

proc WPLoadCGIVarAs {_var _varas} {
    upvar $_varas varas

  if {[catch {cgi_import_as $_var varas} result]
      && [catch {WPCmd set $_var} varas]
      && [catch {cgi_import_cookie_as $_var varas} result]} {
    set varas ""
  }
}

proc WPImport {valname {errstring ""} {default 0}} {
    upvar $valname val

    if {[catch {cgi_import_as $valname val} result]} {
      if {[catch {WPCmd set $valname} val]} {
	if {[catch {cgi_import_cookie_as $valname val} result]} {
	  if {[string length $errstring] > 0} {
	    error "$errstring: $result"
	  } else {
	    set val $default
	  }
	}
      }
    }
}


proc WPExportCookie {name value {scope ""}} {
    global _wp

    cgi_cookie_set $name=$value "path=[file join / $_wp(urlprefix) $scope]"
}


# handle dynamic sizing of images showing thread relationships 
proc WPThreadImageLink {t h} {
    global _wp

    return "<img src=\"[file join $_wp(imagepath) ${t}.gif]\" border=0 align=top height=${h} width=14>"
}


proc WPInactivePage {{reasons ""}} {
  set l {}
  foreach r $reasons {
    append l "<li>$r"
  }

  WPInfoPage "Inactive Session" \
	"[font size=+2 "Web Alpine Session No Longer Active"]" \
      "There are several reasons why a session might become inactive.<ul><li>A bookmarked reference to a Web Alpine page.<li>Failed periodic page reload due to browser/system suspension associated with power saving mode, etc.${l}</ul><p>Please visit the [cgi_link Start] to start a new session."
}

proc WPInfoPage {title exp1 {exp2 ""} {imgurl {}} {exp3 ""}} {
  global _wp

  catch {

    cgi_html {
      cgi_head {
	cgi_title $title
	cgi_stylesheet [file join / $_wp(urlprefix) $_wp(pubdir) standard.css]
      }

      cgi_body {
	cgi_table height="20%" {
	  cgi_table_row {
	    cgi_table_data {
	      cgi_puts [cgi_nbspace]
	    }
	  }
	}

	cgi_center {
	  cgi_table border=0 width=500 cellpadding=3 {
	    cgi_table_row {
	      cgi_table_data align=center rowspan=3 {
		if {[string length $imgurl]} {
		  cgi_put [cgi_url [cgi_imglink logo] $imgurl]
		} else {
		  cgi_put [cgi_imglink logo]
		}
	      }

	      cgi_table_data rowspan=3 {
		cgi_put [nbspace]
		cgi_put [nbspace]
	      }

	      cgi_table_data {
		cgi_puts $exp1
	      }

	    }

	    if {[string length $exp3]} {
	      cgi_table_row {
		cgi_table_data "style=\"border: 1px solid red; background-color: pink\"" {
		  cgi_puts $exp3
		}
	      }
	    }

	    if {[string length $exp2]} {
	      cgi_table_row {
		cgi_table_data {
		  cgi_puts $exp2
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

proc WPimg {image {extension gif}} {
    global _wp

    return [file join $_wp(imagepath) ${image}.${extension}]
}

proc WPCharValue {c} {
  scan "$c" %c n
  return $n
}

proc WPPercentQuote {arg {exclude {}}} {
  set t "\[^0-9a-zA-Z_${exclude}\]"
  if {[regsub -all $t $arg {[format "%%%.2X" [WPCharValue "\\&"]]} subarg]} {
    set x [subst $subarg]
    return $x
  } else {
    return $arg
  }
}

proc WPJSQuote {l} {
  regsub -all {([\\'])} $l {\\\1} l
  return $l
}

proc WPurl {cmd cmdargs text explanation args} {
    global _wp

    lappend urlargs $text
    lappend urlargs $cmd
    if {[regexp "^java*" $cmd] == 0 && [string first . $cmd] < 0} {
	append urlargs ".tcl"
    }

    if {[string length $cmdargs]} {
	if {[set i [string first "?" $cmdargs]] >= 0} {
	    append urlargs "[cgi_quote_url [string range $cmdargs 0 [expr {$i - 1}]]]?[cgi_quote_url [string range $cmdargs [incr i] end]]"
	} else {
	    append urlargs "?[cgi_quote_url $cmdargs]"
	}
    }

    if {$_wp(statushelp)} {
	lappend urlargs [WPmouseover $explanation]
	lappend urlargs "onMouseOut=window.status=''"
    }

    return [eval "cgi_url $urlargs $args"]
}

proc WPMenuURL {cmd cmdargs text explanation args} {
    return [WPurl $cmd $cmdargs $text $explanation class=menubar [join $args]]
}

proc WPGetTDFontSize {{ih 24}} {
    if {$ih <= 20 } {return 12}
    if {$ih >= 30 } {return 24}
    return [expr {$ih - 8}]
}
  
proc WPGetviewFontSize {{ih 24}} {
    if {$ih <= 20 } {return 8}
    if {$ih >= 30 } {return 13}
    return [expr {($ih / 2) - 2}]
}

proc WPIndexLineHeight {{ih 0}} {
  global _wp

  set ih [WPCmd PEInfo indexheight]
  if {[string length $ih] == 0 ||  $ih <= 0} {
    set ih $_wp(indexheight)
  }

  return [expr {($ih < 20) ? 20 : $ih}]
}

proc WPStyleSheets {{ih 0}} {
  global _wp

  cgi_stylesheet [file join / $_wp(urlprefix) $_wp(pubdir) standard.css]

  if {$ih <= 0} {
    set ih [WPIndexLineHeight]
  }

  cgi_puts "<style type='text/css'>\nTD { font-size: [WPGetTDFontSize $ih]px }\n.view {font-size: [WPGetviewFontSize $ih]pt }\n</style>"
  return $ih
}

proc WPStdScripts {{ih 0}} {
    global _wp

    set ih [WPStyleSheets $ih]

    cgi_script language="JavaScript" src="[file join / $_wp(urlprefix) $_wp(pubdir) standard.js]" {}
    cgi_script language="JavaScript1.3" {cgi_put "js_version = '1.3';"}
    cgi_javascript {
	cgi_puts "function getIndexHeight(){return $ih}"
    }
}

proc WPStdHttpHdrs {{ctype {}} {expires 0}} {
    global _wp

    # set date and expires headers the same to prevent caching
    # Date: Tue, 15 Nov 1994 08:12:31 GMT
    set doctime [clock seconds]

    if {[string length $ctype]} {
      cgi_content_type $ctype
    } else {
      cgi_content_type
    }

    cgi_puts "Date: [clock format $doctime -gmt true -format "%a, %d %b %Y %H:%M:%S GMT"]"
    if {$expires == 0} {
      set _wp(nocache) 1
      cgi_puts "Cache-Control: no-cache"
      cgi_puts "Expires: [clock format [expr {$doctime - 31536000}] -gmt true -format "%a, %d %b %Y %H:%M:%S GMT"]"
    } elseif {$expires > 0} {
      cgi_puts "Expires: [clock format [expr {$doctime + ($expires * 60)}] -gmt true -format "%a, %d %b %Y %H:%M:%S GMT"]"
    }
}

proc WPStdHtmlHdr {pagetitle {pagescript ""} {newmail 0}} {
    global _wp

    if {0 && $newmail} {
      set nm "* "
    } else {
      set nm ""
    }

    cgi_title "${nm}Web Alpine - $pagetitle"
    # cgi_base "href=$_wp(serverpath)/"
    if {[info exists _wp(nocache)]} {
      cgi_http_equiv Pragma no-cache
    }

    # cgi_http_equiv Expires $_wp(docdate)
    cgi_meta "name=Web Alpine" content=[clock format [file mtime [info script]] -format "%y%m%d/%H%M"]
    if {[catch {WPCmd set nojs} nojs] || $nojs != 1} {
      cgi_script  type="text/javascript" language="JavaScript" {
	cgi_puts "if(self != top) top.location.href = location.href;"
	cgi_puts "js_version = '1.0';"
      }
    }

    cgi_put  "<link rel=\"icon\" href=\"[cgi_root]/favicon.ico\" type=\"image/x-icon\">"
    cgi_put  "<link rel=\"shortcut icon\" href=\"[cgi_root]/favicon.ico\" type=\"image/x-icon\"> "
}

proc WPHtmlHdrReload {pagescript} {
  global _wp

  if {[regexp {\?} $pagescript]} {
    set c "&"
  } else {
    set c "?"
  }

  cgi_http_equiv Refresh "$_wp(refresh); url=[cgi_root]/${pagescript}${c}reload=1"
}

proc WPNewMail {reload {viewpage msgview.tcl}} {

    if {[catch {WPCmd PEMailbox newmail $reload} newmail]} {
	return -code error $newmail
    }

    set newref ""

    if {[set msgsnew [lindex $newmail 0]] > 0} {
      if {[string length $viewpage]} {
	if {[string first {?} $viewpage] < 0} {
	  set delim ?
	} else {
	  set delim &
	}

	set newurl "${viewpage}${delim}uid=[lindex $newmail 1]"
      } else {
	set newurl [lindex $newmail 1]
      }

      set newicon "postmark"
      set newtext [cgi_quote_html [WPCmd PEMailbox newmailstatmsg]]

      if {[WPCmd PEInfo feature enable-newmail-sound]} {
        #set audio sounds/mail_msg.wav
	set audio /sounds/ding.wav
	if {[isIE]} {
	  set newsound "<bgsound src=\"$audio\" loop=\"1\" volume=\"100\">"
	} else {
	  set newsound "<embed src=\"$audio\" autostart=\"true\" hidden width=0 height=0 loop=\"false\"><noembed><bgsound src=\"$audio\" loop=\"1\"></noembed>"
	}
      } else {
	set newsound {}
      }

      if {0 == [string length $newtext]} {
	set newtext "You have $msgsnew new message[WPplural $msgsnew]"
      }

      lappend newref [list $newtext $newicon $newurl $newsound]
    }

    if {[set deleted [lindex $newmail 2]] > 0} {
	set newtext "$deleted Message[WPplural $deleted] removed from folder"
	lappend newref [list $newtext "" ""]
    }

    foreach statmsg [WPStatusMsgs] {
      lappend newref [list $statmsg "" ""]
      WPCmd PEInfo statmsg ""
    }

    if {!$reload} {
	WPCmd PEMailbox newmailreset
    }

    return $newref
}

proc WPStatusMsgs {} {
  set retmsgs ""
  set lastmsg ""
  if {[catch {WPCmd PEInfo statmsgs} statmsgs] == 0} {
    foreach statmsg $statmsgs {
      if {[string length $statmsg] > 0 && [string compare $statmsg $lastmsg]} {
	if {[regexp "^Pinerc \(.+\) NOT saved$" $statmsg]} {
	  lappend retmsgs "Another Pine/WebPine session may be running.  Settings cannot be saved."
	} else {
	  lappend retmsgs $statmsg
	}

	set lastmsg $statmsg
      }
    }
  }

  return $retmsgs
}

proc WPStatusIcon {uid {extension gif} {statbits ""}} {
  global _wp

  if {[string length $statbits] == 0} {
    set statbits [WPCmd PEMessage $uid statusbits]
  }

  if {[string index $statbits 0]} {
    append sicon "new"
    set alt " N"
    set fullalt "New "
  } else {
    append sicon "read"
    set alt "  "
    set fullalt "Viewed "
  }

  if {[string index $statbits 3]} {
    append sicon "imp"
    set alt "*[string range $alt 1 end]"
    set fullaltend ", important message"
  } elseif {([string index $statbits 4] || [string index $statbits 5])} {
    append sicon "you"
    set alt "+[string range $alt 1 end]"
    set fullaltend " message to you"
  }

  if {[string index $statbits 2]} {
    append sicon "ans"
    set alt "[string range $alt 0 0]A"
    append fullalt ", answered"
  }

  if {[string index $statbits 1]} {
    append sicon "del"
    set alt "[string range $alt 0 0]D"
    append fullalt ", deleted"
  }

  if {[info exists fullaltend]} {
    append fullalt $fullaltend
  } else {
    append fullalt message
  }

  regsub -all { } $alt {\&nbsp;} alt

  return [list [file join $_wp(imagepath) $_wp(staticondir) ${sicon}.${extension}] i_${uid} $alt $fullalt]
}

proc WPStatusLabel {uid} {
    global _wp

    set statbits [WPCmd PEMessage $uid statusbits]

    if {[string index $statbits 0]} {
      set sl new
    } else {
      set sl read
    }

    if {[string index $statbits 3]} {
      set sl important
    }

    if {[string index $statbits 1]} {
      set sl deleted
    }

    if {[string index $statbits 2]} {
      set sl answered
    }

    return $sl
}

proc WPStatusImg {uid} {
    set sicon [WPStatusIcon $uid]
    return [cgi_img [lindex $sicon 0] name=[lindex $sicon 1] id=[lindex $sicon 1] height=16 width=42 border=0 alt=[lindex $sicon 3]]
}

proc WPSessionState {args} {
  switch [llength $args] {
    1 -
    2 {
      if {[catch {WPCmd PEInfo alpinestate} state_list] == 0} {
	array set state_array $state_list
	if {[llength $args] == 1} {
	  return $state_array([lindex $args 0])
	} else {
	  set state_array([lindex $args 0]) [lindex $args 1]
	  set state_list [array get state_array]
	  if {[catch {WPCmd PEInfo alpinestate $state_list} result]} {
	    error "Can't set session state : $result"
	  }
	}
      } else {
	error "Can't read session state"
      }
    }
    default {
      error "Unknown SessionState Parameters: $args"
    }
  }
}

proc WPScriptVersion {tag {inc 0}} {
  if {[catch {WPCmd set wp_script_version} sv]} {
    set versions($tag) [expr int((1000 * rand()))]
    set sv [array get versions]
    catch {WPCmd set wp_script_version $sv}
  } else {
    array set versions $sv

    if {![info exists versions($tag)]} {
      set versions($tag) [expr int((1000 * rand()))]
      set sv [array get versions]
      catch {WPCmd set wp_script_version $sv}
    } elseif {$inc} {
      incr versions($tag) $inc
      set sv [array get versions]
      catch {WPCmd set wp_script_version $sv}
    }
  }

  return $versions($tag)
}

proc WPplural {count} {
    if {$count > 1} {
	return "s"
    }

    return ""
}

proc WPcomma {number {dot ,}} {
    set x ""

    while {[set n [string length $number]] > 3} {
	set x "${dot}[string range $number [incr n -3] end]$x"
	set number [string range $number 0 [incr n -1]]
    }

    return "$number$x"
}

proc isIE {} {
  global env

  return [expr {[info exists env(HTTP_USER_AGENT)] == 1 && [string first MSIE $env(HTTP_USER_AGENT)] >= 0}]
}

proc isW3C {} {
  global env

  return [expr {[info exists env(HTTP_USER_AGENT)] && (([regexp {^Mozilla/([0-9]).[0-9]+} $env(HTTP_USER_AGENT) match majorversion] && $majorversion > 4) || ([regexp {Opera ([0-9])\.[0-9]+} $env(HTTP_USER_AGENT) match majorversion] && $majorversion > 5))}]
}

proc WPdebug {args} {
    global _wp

    switch [lindex $args 0] {
      level {
	if {[regexp {^([0-9])+$} [lindex $args 1]]} {
	  WPSend $_wp(sockname) [subst {PEDebug level [lindex $args 1]}]
	}
      }
      imap {
	switch [lindex $args 1] {
	  on {
	    WPSend $_wp(sockname) [subst {ePEDebug imap 4}]
	  }
	  off {
	    WPSend $_wp(sockname) [subst {PEDebug imap 0}]
	  }
	}
      }
      default {
	WPSend $_wp(sockname) "PEDebug write [list [list [file tail [info script]]: [lrange $args 0 end]]]"
      }
    }
}

proc WPdebugstack {} {
    set stack {}

    for {set n [expr {[info level] - 1}]} {$n > 0} {incr n -1} {
	append stack "$n) [info level $n]\n"
    }
    return $stack
}


##############################################################
# routines to improve integration with cgi.tcl
##############################################################

# routine exposing some of cgi.tcl's innards.
# Should be exported by cgi.tcl package.
proc reset_cgi_state {} {
  global _cgi

  catch {unset _cgi(http_head_in_progress)}
  catch {unset _cgi(http_head_done)}
  catch {unset _cgi(http_status_done)}
  catch {unset _cgi(html_in_progress)}
  catch {unset _cgi(head_in_progress)}
  catch {unset _cgi(head_done)}
  catch {unset _cgi(html_done)}
  catch {unset _cgi(head_suppress_tag)}
  catch {unset _cgi(body_in_progress)}
  catch {unset _cgi(tag_in_progress)}
  catch {unset _cgi(form_in_progress)}
  catch {unset _cgi(close_proc)}

  if {[info exists _cgi(returnIndex)]} {
    while {[set _cgi(returnIndex)] > 0} {
      incr _cgi(returnIndex) -1
      rename cgi_puts ""
      rename cgi_puts$_cgi(returnIndex) cgi_puts
    }
  }
}

###############################################################
# routines to process (and be called in) html template files
###############################################################

proc html_readfile {file} {
    set x [open $file "r"]
    set result [read $x]
    close $x
    return $result
}

proc html_eval {_vars_ _this_} {
    foreach {_i_ _j_} $_vars_ {
	if {$_i_ == "global"} {global $_j_} {set $_i_ $_j_}
    }
    unset _vars_ _i_ _j_
    return [subst $_this_]
}

proc html_loop {varslist text} {
    set result ""
    foreach {vars} $varslist {
	append result [html_eval $vars $text]
    }
    return $result
}

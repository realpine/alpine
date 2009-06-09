# common definitions for all examples

# cgi_debug -on

set NIST_HOST	http://www.nist.gov
set MSID_HOST 	http://www.nist.gov
set EXPECT_HOST	http://expect.nist.gov
set EXPECT_ART	$EXPECT_HOST/art
set MSID_STAFF	$MSID_HOST/msidstaff
set CGITCL	$EXPECT_HOST/cgi.tcl
set DATADIR	data

set domainname "unknown"
catch {set domainname [exec domainname]}

# prevent everyone in the world from sending specious mail to Don!
if {($domainname == "cme.nist.gov") || ([info hostname] == "ats.nist.gov")} {
    cgi_admin_mail_addr libes@nist.gov
}

set TOP target=_top
cgi_link NIST	  "NIST"			$NIST_HOST $TOP
cgi_link Don	  "Don Libes"			$MSID_STAFF/libes $TOP
cgi_link admin    "your system administrator"	mailto:[cgi_admin_mail_addr]
cgi_link CGITCL   "cgi.tcl homepage"		$CGITCL $TOP
cgi_link examples "list of examples"		[cgi_cgi examples] $TOP
cgi_link realapps "real applications"		$CGITCL/realapps.html $TOP
cgi_link Expect   "Expect"			$EXPECT_HOST $TOP
cgi_link Oratcl   "Oratcl"			http://www.nyx.net/~tpoindex/tcl.html#Oratcl $TOP

cgi_imglink logo  $EXPECT_ART/cgitcl-powered-feather.gif align=right "alt=powered-by-cgi.tcl logo"
cgi_link logolink [cgi_imglink logo] $CGITCL $TOP

# Allow for both my development and production environment.  And people
# who copy this to their own server and fail to change cgi_root will get
# my production environment!
if {$domainname == "cme.nist.gov"} {
    cgi_root "http://www-i.cme.nist.gov/cgi-bin/cgi-tcl-examples"
} else {
    cgi_root "http://ats.nist.gov/cgi-bin/cgi.tcl"
}

proc scriptlink {} {
    if {0==[catch {cgi_import framed}]} {
	set target "target=script"
    } else {
	set target ""
    }

    cgi_url "the Tcl script" [cgi_cgi display scriptname=[info script]] $target
}

proc app_body_start {} {
    h2 [cgi_title]
    puts "See [scriptlink] that created this page."
    hr
}

proc app_body_end {} {
    hr; puts "[cgi_link logolink]"
        puts "Report problems with this script to [link admin]."
    br; puts "CGI script author: [link Don], [link NIST]"
    br; puts "Go back to [link CGITCL] or [link examples]."
}

cgi_body_args bgcolor=#00b0b0 text=#ffffff

proc user_error {msg} {
    h3 "Error: $msg"
    cgi_exit
}

# support for rare examples that must be explicitly framed
proc describe_in_frame {title msg} {
    if {0 == [catch {cgi_import header}]} {
	cgi_title $title
	cgi_body {
	    p $msg
	}
	exit
    }
}

#!./tclsh
# $Id: compose.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

# compose.tcl
#
# Purpose: TCL script to generate html form representing
#          a message composition.  

#  Input: 
set compose_vars {
  {uid		""	0}
  {style	""	""}
  {nickto       ""      ""}
  {book         ""      0}
  {ai           ""      -1}
  {notice	""	""}
  {repall	""	0}
  {reptext	""	0}
  {repqstr	""	""}
  {f_name	""	""}
  {f_colid	""	0}
  {cid		"Missing Command ID"}
  {spell	""	""}
  {oncancel	""	"main.tcl"}
}


# inherit global config
source ./alpine.tcl
source cmdfunc.tcl

#  Output: 
#

set bgcolor $_wp(dialogcolor)
set buttoncolor "ffcc66"

# set the default for the compose form. Modern browsers should submit
# in the character set of the HTML form.
set default_charset	"UTF-8"

#set cancelmsg "Really Cancel your Message (and destroy its text)"

set msgs(cancel) "Cancel message (answering OK will abandon your mail message)"

set msgs(noattach) {[ Message has no attachments ]}

set defaultheaders {to cc subject attach}

set fccname ""

proc fieldname {name} {
  regsub -all -- {-} [string tolower $name] {_} fieldname
  return $fieldname
}

proc default_fcc {itemval} {
  global fccdefault f_name f_colid

  if {[catch {WPCmd PEFolder collections} collections]} {
    set collections ""
  }

  # fcc's itemval should be a list of collection-id and folder-name
  if {[llength $itemval] == 2} {
    set fcccol [lindex $itemval 0]
    set fccname [lindex $itemval 1]
  } elseif {[string length $f_name]} {
    set fcccol $f_colid
    set fccname $f_name
  } elseif {[info exists fccdefault] || [catch {WPCmd PECompose fccdefault} fccdefault] == 0} {
    set fcccol [lindex $fccdefault 0]
    set fccname [lindex $fccdefault 1]
    unset fccdefault
  } else {
    set fccname sent-mail
    if {[llength $collections] > 1} {
      set fcccol 1
    } else {
      set fcccol 0
    }
  }

  return [list $fccname $fcccol $collections]
}

proc rowfield {item itemval style} {
  global _wp msgs attachments bgcolor postoption fccname cols

  set litem [string tolower $item]

  if {$cols == 80} {
    set textsize "60%"
  } else {
    set textsize $cols
  }

  cgi_table_row class=body $style bgcolor="$bgcolor" {
    cgi_table_data align=left class=comphdr width=60 {
      switch -- [string tolower $item] {
	attach {
	  #cgi_submit_button "queryattach=Attach" class="comphdrbtn"
	  cgi_put Attachments
	}
	x_fcc {
	  cgi_submit_button "br_fcc=Fcc" class=comphdrbtn
	}
	default {
	  cgi_put $item
	}
      }
    }

    cgi_table_data align=center class=comphdr "style=\"padding: 0px 6px 0px 2px\"" {
      cgi_put [cgi_bold ":"]
    }

    cgi_table_data align=left valign=middle class=comptext "style=\"padding: 2px 0px\"" nowrap {
      switch -- [string tolower $item] {
	attach {
	  cgi_table border=0 cellpadding=0 cellspacing=0 width="100%" {
	    cgi_table_row {
	      set alist ""
	      cgi_table_data align=left {
		if {[info exists attachments] && [set n [llength $attachments]]} {
		  cgi_table border=0 cellpadding=0 cellspacing=0 width="100%" {
		    # format:  {ID TITLE SIZE TYPE}
		    set i 1
		    foreach a $attachments {
		      cgi_table_row {
			if {$n > 1} {
			  cgi_table_data {
			    cgi_puts [cgi_font face=Helvetica "$i)"]
			  }
			  incr i
			}
			cgi_table_data valign=middle align=left {
			  set id [lindex $a 0]
			  cgi_puts [cgi_font face=Helvetica "[lindex $a 1] ([lindex $a 2] bytes) [lindex $a 3]"]
			  lappend alist $id
			}
			cgi_table_data align=right height=23 width=23 {
			  cgi_image_button detach_${id}=[WPimg but_remove] border=0 align=right alt="Delete Attachment"
			}
		      }
		    }
		  }
		} else {
		  cgi_put [cgi_font size="-1" face=Helvetica $msgs(noattach)]
		}

		cgi_text attachments=[join $alist ","] type=hidden notab
	      }

	      cgi_table_data align=right width=70 {
		cgi_submit_button "queryattach=Attach" class=compbtn "style=\"width:64\""
	      }
	    }
	  }
	}
	cc -
	bcc -
	reply-to -
	to {
	  set fn [fieldname $item]
	  cgi_text $fn=${itemval} size=$textsize "style=\"padding: 2px\""
	  cgi_image_button "br_${fn}=[WPimg book]" class=compbtn "alt=Address Book" "style=\"vertical-align:text-bottom\""
	  cgi_submit_button "ex_${fn}=Expand" class=compbtn "style=\"vertical-align:base-line\""
	}
	fcc {
	  set deffcc [default_fcc $itemval]

	  set fccname [lindex $deffcc 0]
	  set fcccol [lindex $deffcc 1]
	  set collections [lindex $deffcc 2]

	  cgi_table border=0 cellpadding=0 cellspacing=0 width="100%" {
	    cgi_table_row {
	      cgi_table_data {
		if {[llength $collections] > 1} {
		  cgi_text [fieldname $item]=$fccname size=38 onFocus=this.select()
		  #cgi_put [cgi_font face=Helvetica "in[cgi_nbspace]collection:[cgi_nbspace]"]
		  cgi_put [cgi_font class=comphdr in][cgi_nbspace]
		  cgi_select colid align=center {
		    for {set j 0} {$j < [llength $collections]} {incr j} {
		      if {$j == $fcccol} {
			set selected selected
		      } else {
			set selected {}
		      }

		      set colname [lindex [lindex $collections $j] 1]
		      if {[string length $colname] > 15} {
			set colname "[string range $colname 0 12]..."
		      }

		      cgi_option $colname value=$j $selected
		    }
		  }
		} else {
		  cgi_text [fieldname $item]=$fccname size=$textsize maxlength=1024 onFocus=this.select()
		  cgi_text "colid=0" type=hidden notab
		}
	      }

	      cgi_table_data align=right {
		cgi_submit_button "br_fcc=Browse" class="compbtn"
	      }
	    }
	  }
	}
	default {
	  cgi_text [fieldname $item]=${itemval} size=$textsize maxlength=1024
	}
      }
    }
  }

  switch -- [string tolower $item] {
    attach {
      cgi_table_row class=smallhdr {
	cgi_table_data colspan=2 {
	  cgi_put [cgi_img [WPimg dot]]
	}

	cgi_table_data {
	  cgi_table cellpadding=0 cellspacing=0 border=0 {
	    cgi_table_row {
	      cgi_table_data valign=middle class=smallhdr {
		# complicated cause prompt and feature are reverse sense
		if {[info exists postoption(fcc-without-attachments)]} {
		  if {$postoption(fcc-without-attachments)} {
		    set checked ""
		  } else {
		    set checked checked
		  }
		} elseif {[WPCmd PEInfo feature "fcc-without-attachments"]} {
		  set checked ""
		} else {
		  set checked checked
		}

		cgi_checkbox fccattach=1 class=smallhdr id="fcccb" $checked
	      }
	      cgi_table_data valign=middle class=smallhdr {
		set blurb "Include attachments in copy of message saved to Fcc"
		if {[string length $fccname]} {
		  append blurb " (i.e., \"$fccname\")"
		}

		cgi_put [cgi_span "style=cursor: pointer" onclick="flipCheck('fcccb')" $blurb]
	      }
	    }
	  }
	}
      }
    }
    default {}
  }
}

set compose_menu {
  {
    {expr [info exists _wp(ispell)] && [file executable $_wp(ispell)]}
    {
      {
	# * * * * SPELLCHECK * * * *
	cgi_put [cgi_img [WPimg dot] width=4]
	cgi_submit_button "check=Spell Check" class=navtext "style=\"margin-top:8\""
      }
    }
  }
  {
    {}
    {
      {
	# * * * * HELP * * * *
	cgi_put [cgi_img [WPimg dot] width=4]
	cgi_submit_button "help=Get Help" class=navtext "style=\"margin-top:8;margin-bottom:8\""
      }
    }
  }
}

set compose2_menu {
  {
    {}
    {
      {
	# * * * * HELP * * * *
	cgi_put [cgi_img [WPimg dot] width=4]
	cgi_submit_button "help=Get Help" class=navtext
      }
    }
  }
}

WPEval $compose_vars {

  if {$cid != [WPCmd PEInfo key]} {
    error [list _close "Invalid Command ID"]
  }

  set cols [WPCmd PEConfig columns]

  switch -- $style {
    Reply {
      set title "Reply to Message [WPCmd PEMessage $uid number]"

      if {[catch {cgi_import part}]} {
	set part ""
      }

      foreach h [WPCmd PEMessage $uid replyheaders $part] {
	set hdrvals([fieldname [lindex $h 0]]) [lindex $h 1]
      }

      if {!$repall} {
	catch {unset hdrvals(cc)}
      }

      if {[WPCmd PEInfo feature quell-format-flowed] == 0} {
	set flowed 1
      }

      if {$reptext} {
	set replytext [WPCmd PEMessage $uid replytext $repqstr $part]
	set body [join [lindex $replytext 0] "\r\n"]
	if {[WPCmd PEInfo feature include-attachments-in-reply]} {
	  set attachments [lindex $replytext 1]
	}
      } else {
	set body ""
	foreach line [WPCmd PEInfo signature] {
	  append body "$line\r\n"
	}
      }

      catch {WPCmd set help_context reply}
    }
    Forward {
      set title "Forward Message [WPCmd PEMessage $uid number]"

      if {[catch {cgi_import part}]} {
	set part ""
      }

      foreach h [WPCmd PEMessage $uid forwardheaders $part] {
	set hdrvals([fieldname [lindex $h 0]]) [lindex $h 1]
      }

      foreach line [WPCmd PEInfo signature] {
	append body "$line\r\n"
      }

      if {[WPCmd PEInfo feature quell-format-flowed] == 0} {
	set flowed 1
      }

      set forwardtext [WPCmd PEMessage $uid forwardtext $part]
      append body [join [lindex $forwardtext 0] "\r\n"]

      set attachments [lindex $forwardtext 1]
      catch {WPCmd set help_context forward}
    }
    Postponed {
      set title "Postponed Message"
      set postponed [WPCmd PEPostpone extract $uid]

      foreach h [lindex $postponed 0] {
	append hdrvals([fieldname [lindex $h 0]]) [lindex $h 1]
      }

      set body [join [lindex $postponed 1] "\r\n"]
      set attachments [lindex $postponed 2]

      foreach opt [lindex $postponed 3] {
	switch [lindex $opt 0] {
	  charset {
	    set charset [lindex $opt 1]
	  }
	}
      }

      unset postponed
      catch {WPCmd set help_context compose}
    }
    default {
      if {![info exists title]} {
	set title "Compose New Message"
      }

      if {[catch {cgi_import ldap}] == 0
	  && [string compare $ldap 1] == 0
	  && [catch {cgi_import qn}] == 0
	  && [catch {cgi_import si}] == 0
	  && [catch {cgi_import ni}] == 0
	  && [catch {WPCmd PELdap ldapext $qn "${si}.${ni}"} leinfo] == 0} {
	foreach item [lindex $leinfo 1] {
	  switch -- [string tolower [lindex $item 0]] {
	    "email address" {
	      if {[catch {cgi_import ei}] == 0} {
		set ldap_email [lindex [lindex $item 1] $ei]
	      } else {
		set ldap_email [lindex [lindex $item 1] 0]
	      }
	    }
	    name {
	      set ldap_name [lindex [lindex $item 1] 0]
	    }
	    "fax telephone" {
	      set ldap_fax [lindex [lindex $item 1] 0]
	    }
	  }
	}

	# put it all together
	if {[catch {cgi_import fax}] == 0 && [string compare $fax yes] == 0} {
	  set n {[0-9]}
	  set n3 $n$n$n
	  set n4 $n$n$n$n
	  if {[info exists ldap_name]
	      && [regexp "^\\\+1 ($n3) ($n3)-($n4)\$" $ldap_fax dummy areacode prefix number]
	      && [lsearch -exact {206 425} $areacode] >= 0} {
	    regsub -all { } $ldap_name {_} ldap_fax_name
	    set hdrvals(to) "\"Fax to ${ldap_name}\" <${ldap_fax_name}@${areacode}-${prefix}-${number}.fax.cac.washington.edu>"
	    set hdrvals(subject) "FAX: "
	  }
	} elseif {[info exists ldap_email]} {
	  if {[info exists ldap_name]} {
	    set hdrvals(to) "\"$ldap_name\" <$ldap_email>"
	  } else {
	    set hdrvals(to) $ldap_email
	  }
	}
      }

      if {[WPCmd PEInfo feature quell-format-flowed] == 0} {
	set flowed 1
      }

      if {![info exists body] || [string length $body] == 0} {
	if {([info exists msgdata] && [llength $msgdata]) || [catch {WPCmd set suspended_composition} msgdata] == 0} {
	  set attachments {}
	  foreach e $msgdata {
	    switch -- [string tolower [lindex $e 0]] {
	      "" {
		# do nothing, empty field
	      }
	      attach {
		if {[catch {WPCmd PECompose attachinfo [lindex $e 1]} ai]} {
		  WPCmd PEInfo statmsg $ai
		} else {
		  lappend attachments [list [lindex $e 1] [lindex $ai 0] [lindex $ai 1] [lindex $ai 2]]
		  # attachment description is in [lindex $ai 3]
		}
	      }
	      body {
		catch {unset body}
		if {[string compare $style Spell] == 0} {
		  if {[catch {cgi_import_as spell spell} result] == 0} {
		    if {[string compare $spell Cancel] == 0} {
		      WPCmd PEInfo statmsg "Spell Check Cancelled"
		    } elseif {[catch {WPCmd set wp_spellresult} spellresult] == 0} {
		      set oldbody [lindex $e 1]
		      foreach l $spellresult {
			set oldline [lindex $oldbody [lindex $l 0]]
			set newline ""
			set offset 0
			foreach w [lindex $l 1] {
			  set o [lindex $w 0]
			  append newline "[string range $oldline $offset [expr {$o - 1}]][lindex $w 2]"
			  set offset [expr {$o + [lindex $w 1]}]
			}

			append newline [string range $oldline $offset end]

			set newlines([lindex $l 0]) $newline
		      }

		      for {set n 0} {$n < [llength $oldbody]} {incr n} {
			if {[info exists newlines($n)]} {
			  append body $newlines($n)
			} else {
			  append body [lindex $oldbody $n]
			}

			append body "\r\n"
		      }
		    } else {
		      WPCmd PEInfo statmsg "No corrections present, Nothing changed."
		    }
		  }

		  catch {WPCmd PEInfo unset wp_spellresult}
		}

		if {![info exists body]} {
		  set body [join [lindex $e 1] "\r\n"]
		}
	      }
	      fcc {
		if {[string length $f_name]} {
		  set hdrvals(fcc) [list $f_colid $f_name]
		} else {
		  set hdrvals(fcc) [lindex $e 1]
		}
	      }
	      postoption {
		set opt [lindex $e 1]
		switch -- [lindex $opt 0] {
		  charset {
		    set charset [lindex $opt 1]
		  }
		  default {
		    set postoption([lindex $opt 0]) [lindex $opt 1]
		  }
		}
	      }
	      default {
		set hdrvals([fieldname [lindex $e 0]]) [lindex $e 1]
	      }
	    }
	  }

	  if {![info exists body]} {
	    set body ""
	  }
	} else {
	  set body ""
	  foreach line [WPCmd PEInfo signature] {
	    append body "$line\r\n"
	  }

	  if {[string length $nickto] != 0 || $ai != -1} {
	    if {[catch {WPCmd PEAddress entry $book $nickto $ai} entryval] == 0} {
	      set addrlist [lindex $entryval 0]
	      set newfcc [lindex $entryval 1]
	      if {[string length $newfcc]} {
		global fccdefault
		if {[string compare $newfcc "\"\""] == 0} {
		  set newfcc ""
		}
		if {[catch {WPCmd PEFolder collections} collections]} {
		  set collections ""
		}
		set fccdefault [list [expr {[llength $collections] > 1 ? 1 : 0}] $newfcc]
	      }
	    } else {
	      set addrlist $nickto
	    }
	    # bug: where's "to" supposed to come from?
	    if {0 && [string length $to]} {
	      append to ", ${addrlist}"
	    } else {
	      set to $addrlist
	    }
	  }

	  WPCmd PECompose noattach
	}
      }

      catch {WPCmd set help_context compose}
    }
  }

  if {[catch {WPCmd PECompose userhdrs} headers]} {
    error [list _action "Header Retrieval Failure" $headers]
  }

  catch {fconfigure stdout -encoding binary}

  if {![info exists charset]} {
    set charset $default_charset
  }

  cgi_http_head {
    WPStdHttpHdrs "text/html; charset=\"$charset\""
  }

  cgi_html {
    cgi_head {
      cgi_http_equiv Content-Type "text/html; charset=$charset"

      WPStdHtmlHdr "Compose Message"
      WPStdScripts
      WPStyleSheets

      cgi_put  "<style type='text/css'>"
      cgi_put  ".comptext	{ background-color: $bgcolor ; font-family: courier, serif ; font-size: 10pt }"
      cgi_put  ".comphdr	{ background-color: $bgcolor ; font-family: arial, sans-serif ; font-size: 10pt ; font-weight: bold }"
      cgi_put  ".comphdrbtn	{ background-color: $bgcolor ; font-family: arial, sans-serif ; font-size: 10pt ; font-weight: bold ; text-decoration: underline }"
      cgi_put  ".comphdrbtnx	{ background-color: $buttoncolor ; font-family: arial, sans-serif ; font-size: 10pt ; font-weight: bold ; text-decorationx: underline }"
      cgi_put  ".compbtn	{ font-family: arial, sans-serif ; font-size: 9pt }"
      cgi_put  ".attachlist	{ width: 300 }"
      cgi_put  ".smallhdr	{ background-color: $bgcolor ; font-family: arial, sans-serif ; font-size: 8pt }"
      cgi_puts "</style>"

      cgi_javascript {
	cgi_put "function setop(i){"
	cgi_put " eval('document.compose.sendop\['+i+'\].checked = true');"
	cgi_put " return false;"
	cgi_put "}"
      }
    }

    cgi_body BGCOLOR="$_wp(bordercolor)" onLoad=document.compose.to.focus() {
      if {[info exists comments]} {
	cgi_puts "Diag: "
	foreach c $comments {
	  cgi_html_comment $c
	}
      }

      cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post name=compose enctype=multipart/form-data target=_top {
	cgi_table border=0 cellspacing=0 cellpadding=0 width=100% {
	  cgi_table_row {
	    lappend msglist [list $notice]
	    # next comes the menu down the left side
	    eval {
	      cgi_table_data $_wp(menuargs) rowspan=2 {

		cgi_division class=navbar "style=background-color:$_wp(menucolor)" {
		  # * * * * SEND * * * *
		  cgi_puts "<fieldset>"
		  set doneverb "   OK   "

		  cgi_puts [cgi_span class=navtext "When finished, choose action below, then click [cgi_italic $doneverb].[cgi_nl][cgi_nl]"]
		  
		  cgi_radio_button sendop=send id="RB1"
		  cgi_put [cgi_span class=navtext "style=color:white; cursor: pointer" onclick=\"flipCheck('RB1')\" "Send"]
		  cgi_br
		  cgi_radio_button sendop=postpone id="RB2"
		  cgi_put [cgi_span class=navtext "style=color:white; cursor: pointer" onclick=\"flipCheck('RB2')\" "Postpone"]
		  cgi_br
		  cgi_radio_button sendop=cancel id="RB3"
		  cgi_put [cgi_span class=navtext "style=color:white; cursor: pointer" onclick=\"flipCheck('RB3')\" "Cancel"]
		  cgi_br
		  cgi_br
		  #cgi_image_button action=[WPimg but_s_do] border=0 alt="Do"
		  cgi_division "style=padding-bottom:4" align=center {
		    cgi_submit_button "action=$doneverb" class=navtext
		  }
		  cgi_puts "</fieldset>"
		}

		WPTFCommandMenu compose_menu {}
	      }
	    }
	  }

	  cgi_table_row {
	    cgi_table_data border=0 bgcolor=$bgcolor {
	      cgi_table {
		cgi_table_row {
		  cgi_table_data {
		    cgi_table border=0 cellspacing=0 cellpadding=0 align=left {
		      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
		      cgi_text "sessid=$sessid" type=hidden notab
		      cgi_text "page=post" type=hidden notab
		      cgi_text "postpost=$oncancel" type=hidden notab
		      if {[string length $repqstr]} {
			cgi_text "repqstr=$repqstr" type=hidden notab
		      }

		      if {0 && [info exists charset]} {
			cgi_text "form_charset=$charset" type=hidden notab
			# character encoding suport: the idea is to plant a known
			# char in the given charset and see what comes back to
			# post.tcl from the browser.  Why is it again the @#*$!
			# browser can't just tell us?
			foreach tc {#8364 #1066 thorn tcedil #65509} {
			  cgi_puts "<input name=\"ke_$tc\" value=&${tc}; type=hidden notab>"
			}
		      }

		      if {[info exists flowed]} {
			cgi_text "form_flowed=yes" type=hidden notab
		      }

		      foreach field [WPCmd PECompose syshdrs] {
			set hdr [fieldname [lindex $field 0]]
			if {[info exists hdrvals($hdr)]} {
			  cgi_text "${hdr}=$hdrvals($hdr)" type=hidden notab
			}
		      }

		      if {[catch {WPCmd set wp_extra_hdrs} extras] == 0 && $extras == 0} {
			set extrahdrs {}
		      }

		      foreach field $headers {
			set item [lindex $field 0]

			if {[string length $item] == 0} {
			  continue
			}

			set itemvaldef [lindex $field 1]
			set litem [string tolower $item]
			set fn [fieldname $item]

			if {[info exists hdrvals($fn)]} {
			  set itemval $hdrvals($fn)
			} elseif {[info exists $litem] && [string length [subst $$litem]]} {
			  set itemval [subst $$litem]
			} elseif {[string length $itemvaldef]} {
			  set itemval $itemvaldef
			} else {
			  set itemval ""
			}

			if {[catch {WPCmd PECompose composehdrs} h] == 0 && [llength $h] > 0} {
			  set display_headers [string tolower $h]
			} else {
			  set display_headers $defaultheaders
			}

			if {![info exists extrahdrs]} {
			  if {[info exists postoption(fcc-set-by-addrbook)]} {
			    if {[lsearch -exact $display_headers fcc] < 0} {
			      lappend display_headers fcc
			    }
			  }
			}

			if {[info exists attachments] && [llength $attachments]
			    && [lsearch -exact $display_headers attach] < 0} {
			  lappend display_headers attach
			}

			if {[lsearch -exact $display_headers [string tolower $item]] >= 0} {
			  rowfield $item $itemval $style
			} elseif {[info exists extrahdrs]} {
			  lappend extrahdrs [list $item $itemval $style]
			} else {
			  switch -- [string tolower $item] {
			    fcc {
			      set deffcc [default_fcc $itemval]

			      set fccname [lindex $deffcc 0]
			      set fcccol [lindex $deffcc 1]

			      cgi_text [fieldname $item]=$fccname type=hidden notab
			      cgi_text "colid=$fcccol" type=hidden notab
			    }
			    default {
			      cgi_text "[fieldname $item]=$itemval" type=hidden notab
			    }
			  }
			}
		      }

		      if {[info exists extrahdrs]} {
			cgi_table_row class=body bgcolor="$bgcolor" {
			  cgi_table_data colspan=4 align=left class=comptext {
			    cgi_image_button extrahdrs=[WPimg hdrless] border=0 alt="Hide Extra Headers"
			  }
			}

			foreach pb $extrahdrs {
			  rowfield [lindex $pb 0] [lindex $pb 1] [lindex $pb 2]
			}
		      } else {
			cgi_table_row class=body bgcolor="$bgcolor" {
			  cgi_table_data colspan=4 align=left class=comptext {
			    cgi_image_button extrahdrs=[WPimg hdrmore] border=0 alt="Show Extra Headers"
			  }
			}
		      }
		    }
		  }
		}

		cgi_table_row {
		  cgi_table_data {
		    cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" {
		      cgi_table_row class=body bgcolor="$bgcolor" {
			cgi_table_data colspan=4 align=center class=comptext {
			  cgi_textarea body=${body} rows=20 cols=$cols wrap=physical class=view "style=\"padding: 2px\""
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

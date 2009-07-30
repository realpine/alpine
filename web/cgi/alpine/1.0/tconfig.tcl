# $Id: tconfig.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

#  tconfig.tcl
#
#  Purpose:  CGI script to configure features/variables

#  Input: 
set conf_vars {
  {newconf {} 0}
  {oncancel "Nothing set for oncancel"}
  {wv      "" general}
  {vlavar  "" ""}
}

#  Output:
#

# read config
source genvars.tcl

set confs {
  {general General g genconf}
  {msgl "Message List" ml mlconf}
  {msgv "Message View" mv mvconf}
  {composer Composer c compconf}
  {address "Address Books" ab abconf}
  {folder "Folders" f fldrconf}
  {rule  "Rules" r ruleconf}
}


set var_menu {
  {
    {}
    {
      {
	# * * * * Save Config * * * *
	#cgi_image_button save=[WPimg but_save] border=0 alt="Save Config"
	cgi_submit_button save=Save
      }
    }
  }
  {
    {}
    {
      {
	# * * * * Close * * * *
	#cgi_image_button cancel=[WPimg but_cancel] border=0 alt="Cancel"
	cgi_submit_button cancel=Cancel
      }
    }
  }
}

## read vars
foreach item $conf_vars {
  if {[catch {cgi_import [lindex $item 0].x}]} {
    if {[catch {eval WPImport $item} result]} {
      if {[llength $item] > 2} {
	set [lindex $item 0] [lindex $item 2]
      } else {
	error [list _action [lindex $item 1] $result]
      }
    }
  } else {
    set [lindex $item 0] 1
  }
}

set type $wv
WPCmd set conf_page $type
switch -- $type {
  msgl {
    set goodvars $msglist_vars
  }
  msgv {
    set goodvars $msgview_vars
  }
  address {
    set goodvars $address_vars
  }
  folder {
    set goodvars $folder_vars
  }
  composer {
    set goodvars $composer_vars
  }
  general {
    set goodvars $general_vars
  }
  rule {
    set goodvars $rule_vars
  }
}
set typeexp "General"

proc button_text {but butno fg bg text} {
  if {[string length $fg]} {
    set fg "color: #${fg};"
  }

  if {[string length $bg]} {
    set bg "background-color: #${bg}"
  }

  cgi_puts "<script><!-- "
  cgi_puts "document.write('<a href=\\\'#\\\' style=\\\"text-decoration: none;\\\" onClick=\"return setop(\\\'${but}\\\',$butno)\">');// -->"
  cgi_puts "</script>"

  cgi_put [cgi_span "style=$fg $bg" $text]

  cgi_puts "<script><!-- "
  cgi_puts "document.write('</a>');// -->"
  cgi_puts "</script>"
}

proc button_checked {def but} {
  if {[string compare $def $but]} {
    return ""
  } else {
    return checked
  }
}

foreach conf $confs {
  if {[string compare $type [lindex $conf 0]] == 0} {
    append ttitle [cgi_imglink "[lindex $conf 2]tab"]
    lappend conftitle [list [cgi_imglink "[lindex $conf 2]tab"] {} {}]
    set typeexp [lindex $conf 1]
    set _wp(helpname) [lindex $conf 3]
  } else {
    append ttitle "<input type=image name=\"[lindex $conf 2]tab\" src=\"[WPimg "tabs/[lindex $conf 2]dtab"]\" border=0 alt=\"[lindex $conf 1]\">";
  }
}

cgi_http_head {
  WPStdHttpHdrs
}

if {$newconf == 1} {
  WPCmd PEConfig newconf
}

cgi_html {
  cgi_head {
    WPStdHtmlHdr "Configuration"
    WPStyleSheets

    cgi_put  "<style type='text/css'>"
    cgi_put  ".vtext { font-size: 10pt; font-family: courier, 'new courier', monospace ; font-weight: medium ; padding-left: 6; border-left: 1px solid black}"
    cgi_put  ".instr { font-size: 10pt; font-weight: bold}"
    cgi_puts "</style>"

    cgi_javascript {
      cgi_put "function setop(b,i){"
      cgi_put " eval('document.varconfig.'+b+'\['+i+'\].checked = true');"
      cgi_put " return false;"
      cgi_put "}"
    }
  }

  cgi_body BGCOLOR="$_wp(bordercolor)" {
    set postjs ""

    cgi_form $_wp(appdir)/$_wp(ui1dir)/wp method=post name=varconfig enctype=multipart/form-data target=_top {
      cgi_text "oncancel=$oncancel" type=hidden notab
      cgi_text "cid=[WPCmd PEInfo key]" type=hidden notab
      cgi_text "page=conf_process" type=hidden notab

      cgi_table width="100%" cellpadding=0 cellspacing=0 {
	cgi_table_row {
	  cgi_table_data width=112 bgcolor=$_wp(menucolor) {
	    cgi_puts [cgi_img [WPimg dot2] width=1 height=1]
	  }
	  cgi_table_data align=right nowrap background="[WPimg [file join tabs tabbg]]" {
	    cgi_put $ttitle
	  }
	}
      }
      if {0} {
      cgi_division align=right  {
	cgi_put $ttitle
      }
      }
      cgi_table border=0 cellspacing=0 cellpadding=0 width="100%" height="100%" {

	cgi_table_row {
	  #
	  # next comes the menu down the left side
	  #
	  eval {
	    cgi_table_data $_wp(menuargs) rowspan=4 {
	      WPTFCommandMenu var_menu {}
	    }
	  }

	  #
	  # In main body of screen goe confg list
	  #
	  cgi_table_data valign=top width="100%" class=dialog "style=\"padding-left: 6\"" {
	    cgi_h2 [cgi_bold "${typeexp} configuration settings:"]
	    switch -- $type {
	      msgl {
		cgi_text "wv=msgl" type=hidden notab
	      }
	      msgv {
		cgi_text "wv=msgv" type=hidden notab
	      }
	      address {
		cgi_text "wv=address" type=hidden notab
	      }
	      folder {
		cgi_text "wv=folder" type=hidden notab
	      }
	      composer {
		cgi_text "wv=composer" type=hidden notab
	      }
	      general {
		cgi_text "wv=general" type=hidden notab
	      }
	      rule {
		cgi_text "wv=rule" type=hidden notab
	      }
	    }
	    set setfeatures [WPCmd PEConfig featuresettings]
	    set icnt 0
	    cgi_table border=0 cellspacing=0 cellpadding=5 width=98% {
	      foreach tmpvar $goodvars {
		set vtypeinp [lindex $tmpvar 0]
		set varname [lindex $tmpvar 1]
		set vardesc [lindex $tmpvar 2]

		if {[catch {WPCmd PEConfig varget $varname}  section] == 0} {
		  set varvals [lindex $section 0]
		  set vartype [lindex $section 1]
		  set vtvals [lindex $section 2]
		  set v_is_default [lindex $section 3]
		  set v_is_fixed [lindex $section 4]
		} else {
		  # UNKNOWN VAR: configure disabled?
		  continue
		}

		switch -- $vtypeinp {
		  var {
		    cgi_table_row {

		      cgi_table_data align=right valign=top nowrap width=50% {
			if {[info exists varname]} {
			  # set script "help.tcl?vn=$varname"
			  # set js "cOpen('$script', 'help', 'scrollbars=yes', 600, 400); return false;"
			  # set js "return vh('$varname')"
			  # cgi_puts [WPurl null "" $varname "" onClick=$js]
			  # cgi_puts [WPurl null "" $vardesc "" onClick=$js]
			  # cgi_submit_button varhelp=$vardesc class=lnkbt
			  cgi_puts [cgi_font class=cfvn $vardesc]
			}
		      }
		      cgi_table_data valign=top {
			cgi_image_button "hlp.$varname=[WPimg cf_help]" alt="Help for $vardesc"
		      }
		      cgi_table_data align=left colspan=2 width=50% {
			switch -- $vartype {
			  listbox {
			    cgi_select $varname align=left {
			      foreach tmpvt $vtvals {
				set tmpvttxt [lindex $tmpvt 0]
				set tmpvtval $tmpvttxt
				if {[llength $tmpvt] > 1} {
				  set tmpvtval [lindex $tmpvt 1]
				}
				if {[string compare $tmpvtval [lindex $varvals 0]]} {
				  if {[llength $tmpvt] > 1} {
				    cgi_option "${tmpvttxt}" "value=${tmpvtval}"
				  } else {
				    cgi_option "${tmpvttxt}" "value=${tmpvttxt}"
				  }
				} else {
				  if {[llength $tmpvt] > 1} {
				    cgi_option "$tmpvttxt" "value=${tmpvtval}" selected
				  } else {
				    cgi_option "$tmpvttxt" selected
				  }
				}
			      }
			    }
			  }
			  textarea {
			    cgi_table border=0 cellpadding=0 cellspacing=0 {
			      set addfield 0
			      set tiwidth 30
			      foreach varval $varvals {
				if {[string length $varval] > [expr {[info exists maxwidth] ? $maxwidth : 0}]} {
				  set maxwidth [string length $varval]
				  incr maxwidth 5
				}
			      }
			      if {[info exists maxwidth]} {
				set tiwidth $maxwidth
			      }
			      if {$tiwidth < 20} {
				set tiwidth 20
			      } elseif {$tiwidth > 50} {
				set tiwidth 50
			      }
			      cgi_table_row {
				cgi_table_data colspan=4 {
				  cgi_image_button vla.$varname=[WPimg cf_add] alt="Add Value"
				}
			      }
			      if {[string compare $vlavar $varname] == 0} {
				set addfield 1
				cgi_table_row {
				  cgi_table_data {
				    cgi_text "varlistadd=$varname" type=hidden notab
				    cgi_text "$varname-add=" type=text size=$tiwidth
				  }
				  cgi_table_data colspan=3 {
				    cgi_puts [cgi_font class=cfntc "(The value entered here will be added.)"]
				  }
				}
			      }
			      set i 0
			      set vvsz [llength $varvals]
			      foreach varval $varvals {
				cgi_table_row {
				  cgi_table_data {
				    cgi_text vle.$varname.$i=$varval type=text size=$tiwidth
				  }
				  cgi_table_data {
				    cgi_image_button vld.$varname.$i=[WPimg cf_delete] alt="Delete Value"
				  }
				  cgi_table_data {
				    if {$i < [expr {$vvsz - 1}]} {
				      cgi_image_button vlsd.$varname.$i=[WPimg cf_shdown] alt="Shuffle Down"
				    } else {
				      cgi_puts [cgi_nbspace]
				    }
				  }
				  cgi_table_data width=50% {
				    if {$i || $addfield} {
				      cgi_image_button vlsu.$varname.$i=[WPimg cf_shup] alt="Shuffle Up"
				    } else {
				      cgi_puts [cgi_nbspace]
				    }
				  }
				}
				incr i
			      }
			      cgi_text "$varname-sz=$i" type=hidden notab
			    }
			  }
			  default {
			    set size [string length [lindex $varvals 0]]
			    if {$size == 0} {
			      set size 20
			    } else {
			      incr size 5
			    }
			    cgi_text "$varname=[lindex $varvals 0]" type=text size=$size tableindex=1
			  }
			}
		      }
		    }
		  }
		  feat {
		    cgi_table_row {
		      cgi_table_data align=right width=50% {
			if {[info exists varname]} {
			  # cgi_submit_button feathelp=$vardesc class=lnkbt
			  cgi_puts [cgi_font class=cfvn $vardesc]
			}
		      }
		      cgi_table_data {
			cgi_image_button "hlp.$varname=[WPimg cf_help]" alt="Help for $vardesc"
		      }
		      cgi_table_data align=left colspan=3 width=50% {
			if {[lsearch $setfeatures $varname] >= 0} {
			  cgi_checkbox $varname checked class=dialog
			} else {
			  cgi_checkbox $varname class=dialog
			}
		      }
		    }

		  }
		  special {
		    switch -- $varname {
		      wp-columns {
			cgi_table_row {
			  cgi_table_data align=right valign=top nowrap {
			    cgi_puts [cgi_font class=cfvn $vardesc]
			  }
			  cgi_table_data valign=top {
			    cgi_image_button "hlp.${varname}=[WPimg cf_help]" alt="Help for $vardesc"
			  }
			  cgi_table_data align=left colspan=2 {
			    set cols [WPCmd PEConfig columns]
			    cgi_select columns align=left {
			      for {set i 20} {$i <= 128} {incr i 4} {
				if {$i == $cols} {
				  cgi_option $i "value=$i" selected
				} else {
				  cgi_option $i "value=$i"
				}
			      }
			    }
			  }
			}
		      }
		      left-column-folders {
			cgi_table_row {
			  cgi_table_data align=right valign=top nowrap {
			    cgi_puts [cgi_font class=cfvn $vardesc]
			  }
			  cgi_table_data valign=top {
			    cgi_puts [cgi_nbspace]
			  }
			  cgi_table_data align=left colspan=2 {
			    if {[catch {WPSessionState left_column_folders} cols]} {
			      set cols $_wp(fldr_cache_def)
			    }

			    cgi_select fcachel align=left {
			      for {set i 0} {$i <= $_wp(fldr_cache_max)} {incr i} {
				if {$i == $cols} {
				  cgi_option $i "value=$i" selected
				} else {
				  cgi_option $i "value=$i"
				}
			      }
			    }
			  }
			}
		      }
		      signature {
			cgi_table_row {
			  cgi_table_data colspan=4 align=center {
			    cgi_puts "<fieldset>"
			    cgi_puts "<legend>Signature</legend>"

			    set rawsig [join [WPCmd PEConfig rawsig] "\n"]
			    cgi_textarea signature=$rawsig rows=8 cols=80 wrap=off

			    cgi_puts "</fieldset>"
			  }
			}

		      }
		      filters -
		      scores -
		      indexcolor -
		      collections {
			set flt 0
			set cll 0
			switch $varname {
			  filters {
			    set flt 1
			    set descsing "Filter"
			    set filts [WPCmd PEConfig filters]
			    set lvals $filts
			    set varhelp 1
			  }
			  scores {
			    set flt 1
			    set descsing "Scores"
			    set filts [WPCmd PEConfig scores]
			    set lvals $filts
			    set varhelp 1
			  }
			  indexcolor {
			    set flt 1
			    set descsing "Index Colors"
			    set filts [WPCmd PEConfig indexcolors]
			    set lvals $filts
			    set varhelp 1
			  }
			  default {
			    set cll 1
			    set descsing "Collection"
			    set colls [WPCmd PEConfig collections]
			    set lvals $colls
			  }
			}
			set tasize [llength $lvals]
			cgi_table_row {
			  cgi_table_data align=center colspan=4 width=50% {
			    cgi_table border=0 cellpadding=3 cellspacing=0 width=100% {
			      cgi_table_row {
				cgi_table_data width=50%  {
				  cgi_puts [cgi_bold $vardesc]
				}
				if {[info exists varhelp]} {
				  cgi_table_data valign=top {
				    cgi_image_button "hlp.$varname=[WPimg cf_help]" alt="Help for $vardesc"
				  }
				}
				cgi_table_data "style=\"padding-left: 10px\"" {
				  cgi_image_button vla.$varname=[WPimg cf_add] alt="Add $descsing"
				}
			      }
			      set i 0
			      foreach lval $lvals {
				cgi_table_row {
				  cgi_table_data "style=\"padding-left: 8px\"" {
				    if {$flt} {
				      cgi_puts [cgi_font class=cfvn "[cgi_nbspace]$lval"]
				    } else {
				      cgi_puts "[cgi_font class=cfvn [lindex $lval 0]]<br>[cgi_span class=cfval "style=margin-left: 8px" [lindex $lval 1]]"
				    }
				  }

				  cgi_table_data "style=\"padding-left: 10px\"" valign=top width=30% {
				    cgi_unbreakable {
				      cgi_image_button vle.$varname.$i=[WPimg cf_edit] alt="Edit $descsing"

				      cgi_image_button vld.$varname.$i=[WPimg cf_delete] alt="Delete $descsing"

				      if {$i < [expr {$tasize - 1}]} {
					cgi_image_button vlsd.$varname.$i=[WPimg cf_shdown] alt="Shuffle Down"
				      } else {
					cgi_puts [cgi_img [WPimg dot2] width=18]
				      }

				      if {$i} {
					cgi_image_button vlsu.$varname.$i=[WPimg cf_shup] alt="Shuffle Up"
				      }
				    }
				  }
				}
				incr i
			      }
			      cgi_text "$varname-sz=$i" type=hidden notab
			    }
			  }
			}
		      }
		      index-format {
			cgi_table_row {
			  cgi_table_data align=left colspan=4 {
			    cgi_puts "<fieldset style=\"margin-left:1%; margin-right:1%\">"
			    #set helpbut [cgi_buffer {cgi_image_button "hlp.$varname=[WPimg cf_help]" alt="Help for $vardesc"}]
			    cgi_puts "<legend>Message Line Format</legend>"

			    set varval [WPCmd PEConfig varget $varname]
			    if {[llength [lindex $varval 0]]} {
			      set fmt ""
			      foreach fms [lindex [lindex $varval 0] 0] {
				if {[regexp {^([a-zA-Z]+[0-9]*)\(([0-9]+[%]?)\)$} $fms dummy f w]} {
				  lappend fmt [list $f $w]
				} elseif {[regexp {^([a-zA-Z]+[0-9]*)$} $fms dummy f]} {
				  lappend fmt [list $f ""]
				}
			      }
			    } else {
			      set fmt [WPCmd PEMailbox indexformat]
			    }

			    cgi_text "index-format=$fmt" type=hidden notab

			    cgi_table cellpadding=0 cellspacing=0 width=100% align=center "bgcolor=#999999" "style=\"border: 1px solid black \"" border=0 {
			      cgi_table_row "bgcolor=#999999" {
				if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
				  cgi_td [cgi_img [WPimg dot2]]
				  cgi_td width=1 [cgi_img [WPimg dot2] width=1]
				}

				foreach fme $fmt {
				  set f [lindex $fme 0]
				  set w [lindex $fme 1]
				  cgi_td xcolspan=3 [cgi_span "style=font-size: 10px; color: white;" $f]
				  if {[regexp {[0123456789]+[%]} $w]} {
				    cgi_td xcolspan=2 align=right [cgi_span "style=font-size: 10px; color: white; padding-right: 4" "(${w})"]
				  } else {
				    cgi_td width=1 [cgi_img [WPimg dot2] width=1]
				  }

				  cgi_td width=1 [cgi_img [WPimg dot2] width=1]
				}
			      }

			      cgi_table_row {
				if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
				  cgi_table_data background="[WPimg bg_index]" align=center valign=middle "style=\"padding-left: 4; padding-right: 4\"" {
				    cgi_checkbox bogus
				  }

				  cgi_td width=1 [cgi_img [WPimg dot2] width=1]
				}

				foreach fme $fmt {
				  set f [lindex $fme 0]
				  set w [lindex $fme 1]

				  switch -regexp $w {
				    [0123456789]+[%] {
				      set width width=$w
				    }
				    "" {
				      set r [WPTFIndexWidthRatio $fmt $f]
				      switch $r {
					1 { set width "" }
					default {
					  set width "width=[expr {round((100/[llength $fmt]) * $r)}]%"
					}
				      }
				    }
				  }

				  set align ""
				  set class ""
				  switch [string toupper [lindex $f 0]] {
				    TO {
				      set varval [WPCmd PEConfig varget personal-name]
				      if {[string length [lindex $varval 0]]} {
					set ftext "To: [lindex $varval 0]"
				      } else {
					set ftext "To: <sender@foo.bar.com>"
				      }
				    }
				    FROM -
				    FROMORTO {
				      set varval [WPCmd PEConfig varget personal-name]
				      if {[string length [lindex $varval 0]]} {
					set ftext [lindex $varval 0]
				      } else {
					set ftext "<sender@foo.bar.com>"
				      }
				    }
				    FULLSTATUS -
				    IMAPSTATUS -
				    STATUS {
				      set ftext "+N"
				      set align "align=center"
				    }
				    MSGNO {
				      set ftext [WPcomma [WPCmd PEMailbox messagecount]]
				      set align "align=center"
				    }
				    SIZE {
				      set ftext [cgi_span class=isize "(1234)"]
				      set class class=isize
				      set align "align=center"
				    }
				    KSIZE {
				      set ftext [cgi_span class=isize "(1K)"]
				      set class class=isize
				      set align "align=center"
				    }
				    SIZECOMMA {
				      set ftext [cgi_span class=isize "(1,234)"]
				      set class class=isize
				      set align "align=center"
				    }
				    DESCRIPSIZE {
				      set ftext "(short+)"
				      set align "align=center"
				    }
				    SIZENARROW {
					set ftext (1K)
				    }
				    DATE {
				      set ftext [clock format [clock seconds] -format "%b %d"]
				      set align "align=center"
				    }
				    DATEISO {
				      set ftext [clock format [clock seconds] -format "%Y-%m-%d"]
				      set align "align=center"
				    }
				    SHORTDATE1 {
				      set ftext [clock format [clock seconds] -format "%m/%d/%y"]
				      set align "align=center"
				    }
				    SHORTDATE2 {
				      set ftext [clock format [clock seconds] -format "%d/%m/%y"]
				      set align "align=center"
				    }
				    SHORTDATE3 {
				      set ftext [clock format [clock seconds] -format "%d.%m.%y"]
				      set align "align=center"
				    }
				    SHORTDATE4 {
				      set ftext [clock format [clock seconds] -format "%y.%m.%d"]
				      set align "align=center"
				    }
				    SHORTDATEISO {
				      set ftext [clock format [clock seconds] -format "%y-%m-%d"]
				      set align "align=center"
				    }
				    SMARTTIME -
				    SMARTDATETIME -
				    TIME12 {
				      regsub {^0*(.*)$} [string tolower [clock format [clock seconds] -format "%I:%M%p"]] {\1} ftext
				      set align "align=center"
				    }
				    TIME24 {
				      set ftext [clock format [clock seconds] -format "%H:%M"]
				      set align "align=center"
				    }
				    TIMEZONE {
				      set ftext [clock format [clock seconds] -format "%z"]
				      set align "align=center"
				    }
				    SUBJECT {
				      set ftext "Re: Config changes..."
				    }
				    ATT {
				      set ftext "1"
				    }
				    CC {
				      set ftext "user@domain"
				    }
				    FROMORTONOTNEWS {
				      set ftext "news.group"
				    }
				    LONGDATE {
				      set ftext [clock format [clock seconds] -format "%b %d, %Y"]
				      set align "align=center"
				    }
				    MONTHABBREV {
				      set ftext [clock format [clock seconds] -format "%b"]
				      set align "align=center"
				    }
				    NEWS {
					set ftext "news.group"
				    }
				    SENDER -
				    RECIPS -
				    NEWSANDTO -
				    RECIPSANDNEWS -
				    NEWSANDRECIPS {
					set ftext "user@domain"
				    }
				    SCORE {
					set ftext "50"
				    }
				    SMARTDATE {
					set ftext "Today"
				    }
				    TOANDNEWS {
					set ftext TOANDNEWS
				    }
				    default {
				      set ftext [lindex $f 0]
				    }
				  }
				  cgi_td $align $class $width nowrap height=34 colspan=2 "style=\"padding-right: 4; padding-left: 4\"" background="[WPimg bg_index]" $ftext
				  cgi_td width=1 [cgi_img [WPimg dot2] width=1]
				}
			      }

			      cgi_table_row {
				if {[WPCmd PEInfo feature enable-aggregate-command-set]} {
				  cgi_table_data "bgcolor=#ffffff" {
				    cgi_put [cgi_img [WPimg dot2]]
				  }

				  cgi_td width=1 [cgi_img [WPimg dot2] width=1]

				  set cols 2
				} else {
				  set cols 0
				}

				set fmturl "wp.tcl?page=conf_process&wv=msgl&adjust=Change&cid=[WPCmd PEInfo key]&oncancel=$oncancel&index-format=[WPPercentQuote $fmt]"
				foreach fme $fmt {
				  cgi_table_data colspan=2 nowrap "bgcolor=#ffffff" align=center valign=middle {
				    set cellurl "${fmturl}&ifield=[lindex $fme 0]"
				    cgi_puts [cgi_url [cgi_img [WPimg if_left] border=0 "alt=Move Field Left" height=11 width=11] "${cellurl}&iop=left" target=_top]
				    cgi_puts [cgi_url [cgi_img [WPimg if_wider] border=0 "alt=Widen Field" height=11 width=11] "${cellurl}&iop=widen" target=_top]
				    cgi_puts [cgi_url [cgi_img [WPimg if_remove] border=0 "alt=Remove Field" height=11 width=11] "${cellurl}&iop=remove" target=_top]
				    cgi_puts [cgi_url [cgi_img [WPimg if_narrow2] border=0 "alt=Narrow Field" height=11 width=11] "${cellurl}&iop=narrow" target=_top]
				    cgi_puts [cgi_url [cgi_img [WPimg if_right] border=0 "alt=Move Field Right" height=11 width=11] "${cellurl}&iop=right" target=_top]
				  }

				  cgi_td width=1 [cgi_img [WPimg dot2] width=1]
				}
			      }
			    }

			    if {[catch {WPCmd PEConfig indextokens} tokens] == 0} {
			      cgi_division align=center {style="margin-top: 16; margin-bottom: 10"} {
				cgi_submit_button "indexadd=Add Field"
				cgi_image_button "hlp.index_tokens=[WPimg cf_help]" alt="Help for $vardesc"
				cgi_select indexaddfield {
				  cgi_option {[  Choose Field to Insert  ]} "value="

				  foreach af [lsort -dictionary $tokens] {
				    if {[lsearch $fmt $af] < 0} {
				      if {[string compare $af MSGNO]} {
					cgi_option $af "value=$af"
				      } else {
					cgi_option $af "value=$af" selected
				      }
				    }
				  }
				}
			      }
			    }

			    cgi_puts "</fieldset>"
			  }
			}
		      }
		      view-colors {
			cgi_table_row {
			  cgi_table_data colspan=4 {
			    cgi_puts "<fieldset style=\"margin-left:1%; margin-right:1%\">"
			    #set helpbut [cgi_buffer {cgi_image_button "hlp.$varname=[WPimg cf_help]" alt="Help for $vardesc"}]
			    cgi_puts "<legend>Color Settings </legend>"

			    cgi_division class=instr "style=\"padding-bottom: 6\"" {
			      cgi_put [cgi_span "style=padding: 2; background-color: #ffcc66; border: 1px solid black" "1"]
			      cgi_put " Choose text below to color, or[cgi_nbspace]enter[cgi_nbspace]field[cgi_nbspace]"
			      cgi_text newfield= class=instr size=12 maxlength=32
			      cgi_put "[cgi_nbspace]to[cgi_nbspace]"
			      cgi_submit_button "addfield=add to colored headers"
			    }

			    set std_hdrs [list Date From To Cc Subject]
			    set samp_hdrs $std_hdrs
			    set samp_text [list normal quote1 quote2 quote3]
			    set colors {}

			    foreach goodkolor $samp_text {
			      set tcolor [WPCmd PEConfig colorget "${goodkolor}"]
			      lappend colors [list $goodkolor [lindex $tcolor 0] [lindex $tcolor 1]]
			    }

			    set hcolors [WPCmd PEConfig colorget "viewer-hdr-colors"]
			    set hi 0
			    foreach hcolor $hcolors {
			      set i 0
			      set dhdr 0
			      foreach samp_hdr $samp_hdrs {
				if {[string compare [string tolower [lindex $hcolor 0]] [string tolower [lindex $samp_hdr 0]]] == 0} {
				  if {[string length [lindex $hcolor 3]] == 0} {
				    switch -- $samp_hdr {
				      Date {
					set samptxt [clock format [clock seconds] -format "%a, %d %b %Y %H:%M:%S %Z"]
				      }
				      From {
					set samptxt [WPCmd PECompose from]
				      }
				      Subject {
					set samptxt "Your colors are Fabulous!"
				      }
				      default {
					set samptxt Sample
				      }
				    }
				    lappend samp_hdr [list [lindex $hcolor 1] [lindex $hcolor 2] $samptxt "" $hi]
				    set samp_hdrs [lreplace $samp_hdrs $i $i $samp_hdr]
				    set dhdr 1
				    break
				  } else {
				    lappend samp_hdr [list [lindex $hcolor 1] [lindex $hcolor 2] [lindex $hcolor 3] [lindex $hcolor 3] $hi]
				    set samp_hdrs [lreplace $samp_hdrs $i $i $samp_hdr]
				    set dhdr 1
				    break
				  }
				}
				incr i
			      }
			      if {$dhdr == 0} {
				set smptxt "Sample"
				if {[string compare "" [lindex $hcolor 3]]} {
				  set smptxt [lindex $hcolor 3]
				}
				lappend samp_hdrs [list [lindex $hcolor 0] [list [lindex $hcolor 1] [lindex $hcolor 2] $smptxt [lindex $hcolor 3] $hi]]
			      }
			      incr hi
			    }

			    set dfgc "ffffff"
			    set dbgc "000000"
			    set dq1fgc "ffffff"
			    set dq1bgc "000000"
			    set dq2fgc "ffffff"
			    set dq2bgc "000000"
			    set dq3fgc "ffffff"
			    set dq3bgc "000000"

			    foreach color $colors {
			      switch -- [lindex $color 0] {
				normal {
				  set dfgc [lindex $color 1]
				  set dbgc [lindex $color 2]
				}
				quote1 {
				  set dq1fgc [lindex $color 1]
				  set dq1bgc [lindex $color 2]
				}
				quote2 {
				  set dq2fgc [lindex $color 1]
				  set dq2bgc [lindex $color 2]
				}
				quote3 {
				  set dq3fgc [lindex $color 1]
				  set dq3bgc [lindex $color 2]
				}
			      }

			      set colarr([lindex $color 0]) [list [lindex $color 1] [lindex $color 2]]
			    }

			    foreach hcolor $hcolors {
			      set colarr([lindex $color 0]) [list [lindex $color 1] [lindex $color 2]]
			    }

			    set butno -1
			    if {[catch {WPCmd PEInfo set config_defground} defground] || ![string length $defground]} {
			      set defground f
			    }

			    if {[catch {WPCmd PEInfo set config_deftext} deftext] || ![string length $deftext]} {
			      set deftext normal
			    }

			    # paint example text
			    cgi_table cellpadding=0 cellspacing=0 width=100% align=center border=0 {
			      cgi_table_row {
				cgi_table_data align=center {

				  cgi_table border=0 cellpadding=0 cellspacing=0 width=100% align=center bgcolor=#${dbgc} "style=\"border-right: 1px solid black\"" {

				    cgi_table_row {
				      cgi_td class=dialog [cgi_img [WPimg dot2]]
				      cgi_td height=1 "bgcolor=#000000" [cgi_img [WPimg dot2] height=1]
				    }

				    foreach samp_hdr $samp_hdrs {
				      set field [string tolower [lindex $samp_hdr 0]]
				      incr butno
				      cgi_table_row {
					cgi_table_data class=dialog align=center nowrap {
					  cgi_radio_button text=hdr.${field} [button_checked $deftext ${field}]
					  if {[llength $samp_hdr] == 1} {
					    set hdrfg $dfgc
					    set hdrbg $dbgc
					  } else {
					    set cp [lindex $samp_hdr end]
					    set hdrfg [lindex $cp 0]
					    set hdrbg [lindex $cp 1]
					  }

					  cgi_text "dfg.${field}=$hdrfg" type=hidden notab
					  cgi_text "dbg.${field}=$hdrbg" type=hidden notab
					}

					cgi_table_data class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc};\"" {
					  cgi_put "[lindex $samp_hdr 0]: "

					  if {[llength $samp_hdr] == 1} {
					    switch -- [lindex $samp_hdr 0] {
					      Date {
						set samptxt [clock format [clock seconds] -format "%a, %d %b %Y %H:%M:%S %Z"]
					      }
					      From {
						set samptxt [WPCmd PECompose from]
					      }
					      default {
						set samptxt Sample
					      }
					    }

					    button_text text $butno $dfgc $dbgc [cgi_quote_html $samptxt]
					    cgi_text "add.${field}=1" type=hidden notab
					  } else {
					    cgi_text "hi.${field}=[lindex [lindex $samp_hdr end] end]" type=hidden notab

					    set pref ""
					    foreach cp [lrange $samp_hdr 1 end] {
					      button_text text $butno $hdrfg $hdrbg [cgi_quote_html "${pref}[lindex $cp 2]"]
					      set pref ", "
					    }
					  }
					}
				      }
				    }

				    if {0} {
				    cgi_table_row {
				      cgi_td class=dialog [cgi_nbspace]
				      cgi_table_data {
					cgi_text newfield= class=vtext
					cgi_submit_button "addfield=Add New Header Field" class=vtext
				      }
				    }
				    }

				    cgi_table_row {
				      cgi_td class=dialog [cgi_nbspace]
				      cgi_td class=vtext [cgi_nbspace]
				    }

				    cgi_table_row {
				      cgi_table_data class=dialog align=center {
					incr butno
					cgi_radio_button text=normal [button_checked $deftext normal]
					cgi_text "dfg.normal=$dfgc" type=hidden notab
					cgi_text "dbg.normal=$dbgc" type=hidden notab
				      }

				      cgi_table_data class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc}\"" {
					button_text text $butno $dfgc $dbgc "This is a rather silly message."
				      }
				    }

				    cgi_table_row {
				      cgi_td class=dialog [cgi_nbspace]
				      cgi_td class=vtext [cgi_nbspace]
				    }

				    cgi_table_row {
				      cgi_td class=dialog [cgi_img [WPimg dot2]]
				      cgi_table_data colspan=2 class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc}\"" {
					cgi_put "On Apr 1, 2001, Sample wrote:</a>"
				      }
				    }

				    cgi_table_row {
				      cgi_table_data class=dialog align=center {
					incr butno
					cgi_radio_button text=quote3 [button_checked $deftext quote3]
					cgi_text "dfg.quote3=[lindex $colarr(quote3) 0]" type=hidden notab
					cgi_text "dbg.quote3=[lindex $colarr(quote3) 1]" type=hidden notab
				      }

				      cgi_table_data class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc}\"" {
					cgi_put [cgi_span "style=color: #[lindex $colarr(quote1) 0]; background-color: #[lindex $colarr(quote1) 1]" "&gt; "]
					cgi_put [cgi_span "style=color: #[lindex $colarr(quote2) 0]; background-color: #[lindex $colarr(quote2) 1]" "&gt; "]
					button_text text $butno [lindex $colarr(quote3) 0] [lindex $colarr(quote3) 1] "&gt; This is an example of text that is quoted 3 times"
				      }
				    }

				    cgi_table_row {
				      cgi_table_data class=dialog align=center {
					incr butno
					cgi_radio_button text=quote2 [button_checked $deftext quote2]
					cgi_text "dfg.quote2=[lindex $colarr(quote2) 0]" type=hidden notab
					cgi_text "dbg.quote2=[lindex $colarr(quote2) 1]" type=hidden notab
				      }

				      cgi_table_data class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc}\"" {
					cgi_put [cgi_span "style=color: #[lindex $colarr(quote1) 0]; background-color: #[lindex $colarr(quote1) 1]" "&gt; "]
					button_text text $butno [lindex $colarr(quote2) 0] [lindex $colarr(quote2) 1] "&gt; This is an example of text that is quoted 2 times"
				      }
				    }

				    cgi_table_row {
				      cgi_table_data class=dialog align=center {
					incr butno
					cgi_radio_button text=quote1 [button_checked $deftext quote1]
					cgi_text "dfg.quote1=[lindex $colarr(quote1) 0]" type=hidden notab
					cgi_text "dbg.quote1=[lindex $colarr(quote1) 1]" type=hidden notab
				      }
				      cgi_table_data class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc}\"" {
					button_text text $butno [lindex $colarr(quote1) 0] [lindex $colarr(quote1) 1] "&gt; This is an example of text that is quoted 1 time"
				      }
				    }

				    if {[llength [WPCmd PEInfo signature]]} {
				      cgi_table_row "style=\"border-right: 1px solid black\"" {
					cgi_td class=dialog [cgi_img [WPimg dot2]]
					cgi_table_data class=vtext "style=\"color: #${dfgc}\; background-color: #${dbgc}\"" {
					  foreach i [WPCmd PEInfo signature] {
					    cgi_puts "$i[cgi_nl]"
					  }
					}
				      }
				    }

				    cgi_table_row {
				      cgi_td class=dialog [cgi_img [WPimg dot2]]
				      cgi_td height=1 "bgcolor=#000000" [cgi_img [WPimg dot2] height=1]
				    }
				  }
				}
			      }

			      cgi_table_row {
				cgi_table_data align=center "style=\"padding-top: 16\"" {
				  cgi_table width=100% border=0 {

				    cgi_table_row colspan=2 {
				      cgi_td valign=top align=left xrowspan=3 class=instr "style=\"padding-right: 4; background-color: #ffcc66; border: 1px solid black\"" [cgi_span "style=padding: 2;" "2"]
				      cgi_td width=30% colspan=2 valign=top class=instr "Choose fore- or background..."
				      cgi_td rowspan=3 width=3% [cgi_img [WPimg dot2]]
				      cgi_td valign=top align=left xrowspan=3 class=instr "style=\"padding-right: 4; background-color: #ffcc66; border: 1px solid black\"" [cgi_span "style=padding: 2;" "3"]
				      cgi_table_data valign=middle class=instr nowrap {
					cgi_put "Choose item's color below, or[cgi_nbspace]"
					cgi_submit_button "reset=restore its default colors"
				      }
				    }

				    cgi_table_row {
				      cgi_td rowspan=2 [cgi_img [WPimg dot2]]
				      cgi_table_data align=right {
					if {[string compare $defground f] == 0} {
					  set checked checked
					} else {
					  set checked ""
					}

					cgi_radio_button ground=f $checked
				      }

				      cgi_table_data "style=\"font-size: 10pt\"" {
					button_text ground 0 "" "" Foreground
				      }

				      cgi_td rowspan=2 [cgi_img [WPimg dot2]]
				      cgi_table_data rowspan=2 {
					cgi_image_button "colormap=[WPimg nondither10x10]" alt="Color Pattern" "style=\"border: 1px solid black\""
				      }
				    }

				    cgi_table_row {
				      cgi_table_data align=right {
					if {[string compare $defground b] == 0} {
					  set checked checked
					} else {
					  set checked ""
					}

					cgi_radio_button ground=b $checked
				      }

				      cgi_table_data "style=\"font-size: 10pt\"" {
					button_text ground 1 "" "" [cgi_span "style=color: black; background-color: #FFCC66; padding: 4" "Background"]
				      }
				    }
				  }
				}
			      }

			      cgi_table_row {
				cgi_table_data colspan=2 align=center class=instr nowrap "style=\"padding-top: 16; padding-bottom: 10;\"" {
				  cgi_puts "Alternatively, you can also "
				  cgi_submit_button "reset=Restore All Color Defaults"
				}
			      }

			      if {0} {
			      cgi_table_row {
				cgi_table_data class=instr nowrap "style=\"padding-top: 16; padding-bottom: 10;\"" {
				  cgi_put [cgi_span "style=padding: 2; background-color: #ffcc66; border: 1px solid black" "OR"]
				  cgi_put " Enter header name[cgi_nbspace]"
				  cgi_text newfield= class=vtext
				  cgi_put "[cgi_nbspace]to[cgi_nbspace]"
				  cgi_submit_button "addfield=Add to Those Being Colored"
				}
			      }
			      }
			    }

			    cgi_puts "</fieldset>"
			  }
			}
		      }
		    }
		  }
		}
		incr icnt
	      }
	    }
	  }
	}
      }
    }
  }
}

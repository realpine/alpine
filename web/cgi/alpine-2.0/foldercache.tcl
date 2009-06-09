# Web Alpine folder cache routines
# $Id$
# ========================================================================
# Copyright 2008 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================




proc saveDefault {{uid 0}} {
  # "size" rather than "number" to work around temporary alpined bug
  if {$uid == 0
      || [catch {WPCmd PEMessage $uid size} n]
      || $n == 0
      || [catch {WPCmd PEMessage $uid savedefault} savedefault]} {
    if {[WPCmd PEFolder isincoming 0]} {
      set colid 1
    } else {
      set colid 0
    }

    return [list $colid [lindex [WPCmd PEConfig varget default-saved-msg-folder] 0]]
  }

  return $savedefault
}

# add given folder name to the cache of saved-to folders
proc addSaveCache {f_name} {
  global _wp

  if {[catch {WPSessionState save_cache} flist] == 0} {
    if {[set i [lsearch -exact $flist $f_name]] < 0} {
      set flist [lrange $flist 0 [expr {$_wp(save_cache_max) - 2}]]
    } else {
      set flist [lreplace $flist $i $i]
    }

    set flist [linsert $flist 0 $f_name]
  } else {
    set flist [list $f_name]
  }

  catch {WPSessionState save_cache $flist}
}

# return the list of cached saved-to folders and make sure given
# default is somewhere in the list
proc getSaveCache {{def_name ""}} {

  if {![string length $def_name]} {
    set savedef [saveDefault 0]
    set def_name [lindex $savedef 1]
  }

  set seen ""

  if {[catch {WPSessionState save_cache} flist] == 0} {
    foreach f $flist {
      if {[string compare $def_name $f] == 0} {
	set def_listed 1
      }

      if {[string length $f] && [lsearch -exact $seen $f] < 0} {
	lappend options $f
	lappend options $f
	lappend seen $f
      }
    }
  }

  if {!([info exists options] && [info exists def_listed])} {
    lappend options $def_name
    lappend options $def_name
  }

  if {[catch {WPCmd set wp_cache_folder} wp_cache_folder]
      || [string compare $wp_cache_folder [WPCmd PEMailbox mailboxname]]} {
    # move default to top on new folder
    switch -- [set x [lsearch -exact $options $def_name]] {
      0 { }
      default {
	if {$x > 0} {
	  set options [lreplace $options $x [expr {$x + 1}]]
	}

	set options [linsert $options 0 $def_name]
	set options [linsert $options 0 $def_name]
      }
    }

    catch {WPCmd set wp_cache_folder [WPCmd PEMailbox mailboxname]}
  }

  return $options
}

# add given folder name to the visited folder cache
proc addFolderCache {f_col f_name} {
  global _wp

  if {$f_col != 0 || [string compare [string tolower $f_name] inbox]} {
    if {0 == [catch {WPSessionState folder_cache} flist]} {

      if {[catch {WPSessionState left_column_folders} fln]} {
	set fln $_wp(fldr_cache_def)
      }

      for {set i 0} {$i < [llength $flist]} {incr i} {
	set f [lindex $flist $i]
	if {$f_col == [lindex $f 0] && 0 == [string compare [lindex $f 1] $f_name]} {
	  break
	}
      }

      if {$i >= [llength $flist]} {
	set flist [lrange $flist 0 $fln]
      } else {
	set flist [lreplace $flist $i $i]
      }

      set flist [linsert $flist 0 [list $f_col $f_name]]
      # let users of data know it's changed (cheaper than hash)
      WPScriptVersion common 1
    } else {
      catch {unset flist}
      lappend flist [list $f_col $f_name] [list [saveDefault 0]]
      # ditto
      WPScriptVersion common 1
    }

    catch {WPSessionState folder_cache $flist}
  }
}

proc removeFolderCache {f_col f_name} {
  global _wp

  if {$f_col != 0 || [string compare [string tolower $f_name] inbox]} {
    if {0 == [catch {WPSessionState folder_cache} flist]} {

      if {[catch {WPSessionState left_column_folders} fln]} {
	set fln $_wp(fldr_cache_def)
      }

      for {set i 0} {$i < [llength $flist]} {incr i} {
	set f [lindex $flist $i]
	if {$f_col == [lindex $f 0] && 0 == [string compare [lindex $f 1] $f_name]} {
	  set flist [lreplace $flist $i $i]
	  WPScriptVersion common 1
	  catch {WPSessionState folder_cache $flist}
	  return
	}
      }
    }
  }
}

# return the list of cached visited folders and make sure given
# default is somewhere in the list
proc getFolderCache {} {
  if {[catch {WPSessionState folder_cache} flist]} {
    catch {unset flist}
    lappend flist [saveDefault 0]
    catch {WPSessionState folder_cache $flist}
  }

  return $flist
}


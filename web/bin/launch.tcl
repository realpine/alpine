#!./tclsh
# $Id: launch.tcl 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $
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

#  Generate a session key, create the connection points (fifos),
#  spawn the mail engine and then hand the session key to the
#  caller

# Source config information
source ./alpine.tcl

# generate session id
WPValidId

if {[info exists env(REMOTE_USER)]} {
  set servlet $_wp(pc_servlet)
  set env(LOGNAME) $env(REMOTE_USER)
} else {
  set servlet $_wp(servlet)
}

set cmd "exec -- echo $_wp(sockname) | [file join $_wp(bin) $servlet]"

# set debug level and configure dmalloc
#append cmd " -d -d -d -d -d -d -d"
#set env(DMALLOC_OPTIONS) "check-fence,check-heap,check-blank,log=/tmp/logfile.%d"

if {[catch {eval $cmd} errmsg]} {
    puts stderr "Unable to Launch servlet: $errmsg"
    exit 1
} elseif {[info exists env(REMOTE_ADDR)] && [string length $env(REMOTE_ADDR)]} {
  catch {WPCmd PEInfo set wp_client $env(REMOTE_ADDR)}
} elseif {[info exists env(REMOTE_HOST)] && [string length $env(REMOTE_HOST)]} {
  catch {WPCmd PEInfo set wp_client $env(REMOTE_HOST)}
}

if {$_wp(debug) > 0} {
    WPCmd PEDebug level $_wp(debug)
}

puts $_wp(sessid)
exit 0

#!./tclsh


#
# and any global config
#

source ./alpine.tcl

cgi_eval {

  cgi_input

  if {[catch {cgi_import serverid}]} {
    set serverid 0
  }
  
  catch {cgi_import logerr}
  
  cgi_http_head {
    WPStdHttpHdrs

    # clear cookies
    cgi_cookie_set sessid=0 expires=now
  }

  if {[info exists env(REMOTE_USER)]} {
    set log_text [font class=notice "Protect your privacy![cgi_nl]When you finish, [cgi_url "completely exit your Web browser" http://www.washington.edu/computing/web/logout.html class=notice]."]
    append log_text "[cgi_nl][cgi_nl]Or you may want to:"
    append log_text "<center><ul>"
    if {[catch {cgi_import ppg}]} {
      set perpage ""
    } else {
      set perpage "&ppg=$ppg"
    }

    append log_text "<li>[cgi_url "restart Web Alpine" "http://alpine.washington.edu"]"
    append log_text "<li>[cgi_url "go to MyUW" "http://myuw.washington.edu"]"
    append log_text "</ul></center>"
    set log_url ""
  } else {
    set log_text "Please visit the [cgi_link Start] for a new session."
    set log_url $_wp(serverpath)/
  }

  if {[info exists logerr] && [string length $logerr]} {
    set log_text "[cgi_bold "Please Note"]: A problem, \"$logerr\", occurred while ending your session.<p>${log_text}"
  }

  WPInfoPage "Logged Out" "[font size=+2 face=Helvetica "Thank you for using Alpine"]" $log_text $log_url
}

# common definitions for all password-related pages

proc password {prompt varname} {
    br
    put "$prompt password: "
    cgi_text $varname= type=password size=16
}

proc successmsg {s} {h3 "$s"}
proc errormsg   {s} {h3 "Error: $s"}

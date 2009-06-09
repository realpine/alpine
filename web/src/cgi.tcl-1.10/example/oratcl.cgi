#!/depot/path/tclsh

# This is a CGI script that demonstrates how easy it is to use a web
# page to query an Oracle server - using cgi.tcl and Oratcl.
# This example fetches the date from Oracle.

# I wish we had a public account on our Oracle server so that I could
# allow anyone to run this, but alas we don't.  So you'll have to
# trust me that it works. - Don

package require cgi
package require Oratcl

cgi_eval {
    source example.tcl

    cgi_title "Oracle Example"
    cgi_input

    cgi_body {
	set env(ORACLE_SID) fork
	set env(ORACLE_HOME) /u01/oracle/product/7322

	set logon [oralogon [import user]  [import password]]
	set cursor [oraopen $logon]

	orasql $cursor "select SysDate from Dual"
	h4 "Oracle's date is [orafetch $cursor]"

	oraclose $cursor
	oralogoff $logon
    }
}

# $Id: about.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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
cgi_h2 "About Web Alpine"
cgi_p "Version [WPCmd PEInfo version].[WPCmd PEInfo revision] (basic HTML interface)"
cgi_p {
Web Alpine is a mail user agent, built on the Alpine Mail System, designed to
provide email access and management facilities via the World Wide Web.  
As a web-based application, Alpine provides universal, convenient, and
secure access to your email environment.
}
cgi_p {Because its foundation is shared
with the Alpine Mail System, Web Alpine can easily provide a view of your email
environment consistent with that of Alpine and PC-Alpine.  However, due to
inherent speed and efficiency limitations of web email, it is not intended
to replace any other email programs.  Web Alpine is designed specifically for
those away from their own desktop computers and for people with light duty
email needs.
}
cgi_p {
}
cgi_p "Send comments and suggestions to [cgi_quote_html "<$_wp(comments)>"]."

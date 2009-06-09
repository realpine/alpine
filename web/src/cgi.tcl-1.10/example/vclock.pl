#!/usr/local/bin/perl

# This script is the "Virtual Clock" example in the seminal
# paper describing CGI.pm, a perl module for generating CGI.
# Stein, L., "CGI.pm: A Perl Module for Creating Dynamic HTML Documents
# with CGI Scripts", SANS 96, May '96.

# Do you think it is more readable than the other version?
# (If you remove the comments and blank lines, it's exactly
# the same number of lines.) - Don

use CGI;
$q = new CGI;

if ($q->param) {
    if ($q->param('time')) {
	$format = ($q->param('type') eq '12-hour') ? '%r ' : '%T';
    }

    $format .= '%A ' if $q->param('day');
    $format .= '%B ' if $q->param('month');
    $format .= '%d ' if $q->param('day-of-month');
    $format .= '%Y ' if $q->param('year');
} else {
    $format = '%r %A %B %d %Y';
}

$time = `date '+$format'`;

# print the HTTP header and the HTML document
print $q->header;
print $q->start_html('Virtual Clock');
print <<END;
<H1>Virtual Clock</H1>
At the tone, the time will be <STRONG>$time</STRONG>.
END

print <<END;
<HR>
<H2>Set Clock Format</H2>
END

# Create the clock settings form
print $q->start_form;
print "Show: ";
print $q->checkbox(-name->'time',-checked=>1);
print $q->checkbox(-name->'day',-checked=>1);
print $q->checkbox(-name->'month',-checked=>1);
print $q->checkbox(-name->'day-of-month',-checked=>1);
print $q->checkbox(-name->'year',-checked=>1);
print "<P>Time style:";
print $q->radio_group(-name=>'type',
		      -values=>['12-hour','24-hour']),"<P>";
print $q->reset(-name=>'Reset'),
      $q->submit(-name=>'Set');
print $q->end_form;

print '<HR><ADDRESS>webmaster@ferrets.com</ADDRESS>'
print $q->end_html;

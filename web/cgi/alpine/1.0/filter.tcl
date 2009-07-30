# $Id: filter.tcl 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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

#  filter.tcl
#
#  Purpose:  Common filter management data/routines

set pattern_id {
    nickname		{"Nickname"				freetext}
    comment		{"Comment"				freetext}
}

set pattern_fields {
    to			{"To"					freetext}
    from		{"From"					freetext}
    sender		{"Sender"				freetext}
    cc			{"Cc"					freetext}
    recip		{"Recipients"				freetext}
    partic		{"Participants"				freetext}
    news		{"Newsgroups"				freetext}
    subj		{"Subject"				freetext}
    alltext		{"All Text"				freetext}
    bodytext		{"Body Text"				freetext}
    age			{"Age Interval"				freetext}
    size		{"Size Interval"			freetext}
    score		{"Score Interval"			freetext}
    keyword		{"Keyword"				freetext}
    charset		{"Character Set"			freetext}
    headers		{"Extra Headers"			headers}
    stat_new		{"Message is New"			status}
    stat_rec		{"Message is Recent"			status}
    stat_del		{"Message is Deleted"			status}
    stat_imp		{"Message is Important"			status}
    stat_ans		{"Message is Answered"			status}
    stat_8bitsubj	{"Subject contains raw 8bit characters"	status}
    stat_bom		{"Beginning of Month"			status}
    stat_boy		{"Beginning of Year"			status}
    addrbook		{"Address in address book"		addrbook}
}


set pattern_actions {
    kill {"kill"}
    folder {"Folder"}
    move_only_if_not_deleted {"moind"}
}

set pattern_colors {
    indexcolor {indexcolor}
}

set pattern_scores {
    scores {scores}
}

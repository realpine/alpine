# $Id: genvars.tcl 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
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

set general_vars {
    {var personal-name    "Name"}
    {var user-domain	  "User Domain"}
    {var inbox-path       "Inbox Location"}
    {var default-fcc      "Default Fcc"}
    {var postponed-folder "Default Postponed Folder"}
    {var alt-addresses    "Other Addresses"}
    {var wp-indexheight   "Font Size"}
    {var smtp-server      "SMTP Server"}
    {feat enable-newmail-sound "Enable Background Sound on New Mail Arrival"}
    {feat enable-flag-cmd "Enable Flag Command"}
    {feat auto-move-read-msgs "Automatically Move Read Messages"}
    {feat expunge-without-confirm "Expunge INBOX Without Confirming"}
    {feat expunge-without-confirm-everywhere "Expunge Everywhere Without Confirming"}
    {feat quit-without-confirm "Quit Without Confirming"}
    {feat enable-jump-cmd "Enable Jump Command"}
    {special wp-columns "Display Width"}
    {special left-column-folders "Message View/List Folder List Count"}
}

set msglist_vars {
    {special index-format     "Message Line Format"}
    {var sort-key "Sort By"}
    {var incoming-startup-rule  "Start At"}
    {var wp-indexlines	  "Message Lines Displayed"}
    {feat mark-for-cc     "Mark Messages For Cc"}
    {feat enable-aggregate-command-set "Enable Aggregate Commands"}
    {feat auto-zoom-after-select "Zoom View after Search"}
    {feat auto-unselect-after-apply "Unmark Messages After Command"}
}

set composer_vars {
    {special signature "Signature"}
    {var default-composer-hdrs "Default Composer Headers"}
    {var customized-hdrs "Customized Headers"}
    {var fcc-name-rule "Fcc name rule"}
    {var empty-header-message "Empty Header Message"}
    {var posting-character-set    "Posting Character Set"}
    {feat compose-rejects-unqualified-addrs "Compose Rejects Unqualified Addresses"}
    {feat enable-sigdashes "Enable Sigdashes"}
    {feat quell-user-lookup-in-passwd-file "Don't Look Up Users in passwd File"}
    {var reply-indent-string "Reply Indent String"}
    {feat enable-reply-indent-string-editing "Enable Reply Indent String Editing"}
    {var reply-leadin "Reply Leadin"}
    {feat include-attachments-in-reply "Include Attachments in Reply"}
    {feat include-header-in-reply "Include Header in Reply"}
    {feat include-text-in-reply "Include Text in Reply"}
    {feat signature-at-bottom "Signature at Bottom"}
    {feat strip-from-sigdashes-on-reply "Strip From Sigdashes on Reply"}
    {feat fcc-without-attachments "Fcc Without Attachments"}
    {feat enable-8bit-esmtp-negotiation "Enable 8bit ESMTP Negotiation"}
    {feat enable-delivery-status-notification "Enable Delivery Status Notification"}
    {feat enable-verbose-smtp-posting "Enable Verbose SMTP Posting"}
    {feat use-sender-not-x-sender "Use Sender, Not X-Sender"}
    {feat send-confirms-only-expanded "Confirm Send only if Addresses Expanded"}
    {feat quell-content-id "Prevent Content-ID header in Attachments"}
    {feat quell-format-flowed "Do Not Send Flowed Text"}
    {feat forward-as-attachment "Forward message as attachment"}
}

set folder_vars {
    {special collections "Folder Collections"}
    {var default-fcc "Default Fcc"}
    {var default-saved-msg-folder "Default Saved Message Folder"}
    {var saved-msg-name-rule "Saved Message Name Rule"}
    {var postponed-folder "Postponed Folder"}
    {var read-message-folder "Read Messages Folder"}
    {var form-letter-folder "Form Letter Folder"}
    {var folder-sort-rule "Folder Sort Rule"}
    {var incoming-folders "Incoming Folders"}
    {var pruned-folders "Pruned Folders"}
    {var pruning-rule "Pruning Rule"}
    {feat prune-uses-yyyy-mm "Prune Uses YYYY-MM"}
    {feat enable-dot-folders "Enable Hidden Folders"}
    {feat enable-lame-list-mode "Enable Lame List Mode"}
    {feat try-alternative-authentication-driver-first "Try Alternative Authentication First"}
    {feat quell-empty-directories "Do Not Display Empty Directores"}
}

set address_vars {
    {var address-book "Address Books"}
    {var global-address-book "Global Address Books"}
    {var addrbook-sort-rule "Address Book Sort Rule"}
    {var ldap-servers "Directory Servers"}
}

set msgview_vars {
    {special view-colors "Message View Color Settings"}
    {var viewer-hdrs "Viewer Headers"}
    {feat enable-msg-view-urls "Enable URLs"}
    {feat enable-msg-view-web-hostnames "Enable Web Hostnames"}
    {feat enable-msg-view-addresses "Enable Address Links"}
    {feat enable-msg-view-attachments "Enable Attachments View"}
    {feat enable-full-header-cmd "Enable Full Headers"}
    {feat quell-host-after-url "Hide server name display after links in HTML"}
}

set rule_vars {
    {special filters "Filters"}
    {special scores "Scoring"}
    {special indexcolor "Index Colors"}
}

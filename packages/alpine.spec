Summary: University of Washington Pine mail user agent
Name: alpine
Version: 2.01
Release: 1
License: Apache License
Group: Applications/Mail
Source: ftp://ftp.cac.washington.edu/alpine/%{name}-%{version}.tar.gz
URL: http://www.washington.edu/alpine
Vendor: University of Washington
Packager: Alpine Team <alpine-contact@u.washington.edu>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
Alpine -- an Alternatively Licensed Program for Internet
News & Email -- is a tool for reading, sending, and managing
electronic messages.  Alpine is the successor to Pine and was
developed by Computing & Communications at the University of
Washington.  
  Though originally designed for inexperienced email users,
Alpine supports many advanced features, and an ever-growing number of
configuration and personal-preference options.

%prep
%setup -q -n %{name}-%{version}

%build
touch imap/ip6
./configure --without-krb5 --with-spellcheck-prog=aspell
make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
install -D -m755 alpine/alpine $RPM_BUILD_ROOT%{_bindir}/alpine
install -D -m755 pico/pico $RPM_BUILD_ROOT%{_bindir}/pico
install -D -m755 pico/pilot $RPM_BUILD_ROOT%{_bindir}/pilot
install -D -m755 alpine/rpload $RPM_BUILD_ROOT%{_bindir}/rpload
install -D -m755 alpine/rpdump $RPM_BUILD_ROOT%{_bindir}/rpdump
install -D -m755 imap/mailutil/mailutil $RPM_BUILD_ROOT%{_bindir}/mailutil
if ! install -D -m2755 -gmail imap/mlock/mlock $RPM_BUILD_ROOT%{_sbindir}/mlock; then
install -D -m755 imap/mlock/mlock $RPM_BUILD_ROOT%{_sbindir}/mlock
echo "*** DO NOT FORGET TO DO THE FOLLOWING BY HAND while root:
***  chgrp mail $RPM_BUILD_ROOT%{_sysconfdir}/mlock
***  echo chmod 2755 $RPM_BUILD_ROOT%{_sysconfdir}/mlock"
fi
install -D -m644 doc/alpine.1 $RPM_BUILD_ROOT%{_mandir}/man1/alpine.1
install -D -m644 doc/pico.1 $RPM_BUILD_ROOT%{_mandir}/man1/pico.1
install -D -m644 doc/pilot.1 $RPM_BUILD_ROOT%{_mandir}/man1/pilot.1
install -D -m644 doc/rpload.1 $RPM_BUILD_ROOT%{_mandir}/man1/rpload.1
install -D -m644 doc/rpdump.1 $RPM_BUILD_ROOT%{_mandir}/man1/rpdump.1
install -D -m644 imap/src/mailutil/mailutil.1 $RPM_BUILD_ROOT%{_mandir}/man1/mailutil.1

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README LICENSE doc/tech-notes.txt
%{_bindir}/alpine
%{_bindir}/pico
%{_bindir}/pilot
%{_bindir}/rpload
%{_bindir}/rpdump
%{_bindir}/mailutil
%attr(2755, root, mail) %{_sbindir}/mlock
%{_mandir}/man1/alpine.1*
%{_mandir}/man1/pico.1*
%{_mandir}/man1/pilot.1*
%{_mandir}/man1/rpload.1*
%{_mandir}/man1/rpdump.1*
%{_mandir}/man1/mailutil.1*

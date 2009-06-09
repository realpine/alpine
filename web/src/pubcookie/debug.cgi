#!/usr/bin/perl
$| = 1;
$wp_uidmapper_bin = "/usr/local/libexec/alpine/bin/wp_uidmapper";
$wp_uidmapper_socket = "/tmp/wp_uidmapper";

print "Content-type: text/plain\n\n";
print "klist:\n";
system("/usr/local/bin/klist");
print "\n";

#if($ENV{'QUERY_STRING'} eq 'stop') {
#    foreach $line (ps("axww")) {
#	($line =~ m/^(\d+).*wp_uidmapper/) && kill(15,$1);
#    }
#    sleep(1);
#    if(-e $wp_uidmapper_socket) {
#	print "Could not kill wp_uidmapper\n";
#	exit(1);
#    }
#    print "wp_uidmapper stopped\n";
#    exit 0;
#}

if (! -e $wp_uidmapper_socket) {
    print "Yikes! need to spawn new wp_uidmapper process\n";
    if(!fork()) {
	close(STDIN);
	close(STDOUT);
	close(STDERR);
	setpgrp(0,0);
	exec("$wp_uidmapper_bin 60000-64999");
    }
    sleep 1;
}

@ps = ps("auxww");
print "wp_uidmapper:\n";
foreach $line (@ps) { ($line =~ m/wp_uidmapper/) && print $line; }

print "\nEnvironment:\n";
foreach $key (sort { $a cmp $b } keys(%ENV)) {
    print "$key: $ENV{$key}\n";
}

print "\nAlpine user processes:\n";
foreach $line (@ps) { ($line =~ m/^\#/) && print $line; }
exit 0;

sub ps {
    my ($args) = @_;
    my (@a);
    open(PS,"ps $args|") || return;
    @a = <PS>;
    close(PS);
    return @a;
}



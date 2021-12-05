#!/usr/bin/env perl

package TestUtil;

use strict;
use warnings;

use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(status);

use Term::ANSIColor qw(colored);


sub status {
    (my $name, my $exp, my $act, my $ok, my $verb) = @_;
    print "$name ";
    if ($verb) {
        print colored("[$exp expected vs $act actual] ", 'grey16');
    }
    print(($ok) ? colored("OK", 'green') : colored("FAIL", 'red'), "\n");
    if (not $ok) {
        print colored("expected '$exp', got '$act'", 'red'), "\n";
    }
}

1;

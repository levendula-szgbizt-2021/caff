#!/usr/bin/env perl

package Ciff;

use strict;
use warnings;
use feature qw(switch);
no warnings qw(experimental::smartmatch);

use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(genciff);

use constant {
    MAGIC => 'CIFF',
};


# usage: genciff(
#   width,      # width of resulting image
#   height,     # height of resulting image
#   caption,    # caption of resulting image
#   tags,       # ref to an array of strings (tags)
#   type        # one of {red, huflag, itflag, danger} (string)
# )
sub genciff {
    my ($width, $height, $caption, $tagsRef, $type) = @_;
    my @tags = @$tagsRef;

    my $size = 3 * $width * $height;
    my $tagstr = join("\0", @tags) . "\0";

    $caption .= "\n";

    my $hdrsize = 4 + 4 * 8 + (length $caption) + (length $tagstr);
    #             ^     ^
    #           magic   |
    #                8 byte nums

    my $hdr = pack('A4Q4', MAGIC, $hdrsize, $size, $width, $height)
      . $caption
      . $tagstr;

    my @pixels;
    for ($type) {
        when ('red') { @pixels = ((255, 0, 0) x $width) x $height }
        when ('huflag') {
            @pixels = (
                ((255, 0, 0) x $width) x ($height/3),       # red
                ((255, 255, 255) x $width) x ($height/3),   # white
                ((0, 255, 0) x $width) x ($height/3),       # green
            );
        }
        when ('itflag') {
            @pixels = (
                (255, 0, 0) x ($width/3),       # red
                (255, 255, 255) x ($width/3),   # white
                (0, 255, 0) x ($width/3),       # green
                (0, 0, 0) x ($width % 3),       # pad
            ) x $height;
        }
        when ('danger') {
            @pixels = (
                ((255, 255, 0) x $width) x ($height/6/2),
                ((0, 0, 0) x $width) x ($height/6/2)
            ) x ($height/2);
        }
        default { die "Unknown type $type" }
    }
    my $data = pack("C$size", @pixels);

    return $hdr . $data;
}


1;

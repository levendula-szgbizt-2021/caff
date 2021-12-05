#!/usr/bin/env perl

package Caff;

use strict;
use warnings;
no warnings qw(experimental::smartmatch);
use feature qw(switch);

use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(gencaff);

use Cwd qw(abs_path);
use File::Basename qw(dirname);
use lib dirname(abs_path($0)); # add ./ to @INC

use DateTime;

use Ciff qw(genciff);

use constant {
    MAGIC => 'CAFF',
    HDRLEN => 4 + 8 + 8,    # magic + length field + frame count field
};


# usage: genblock(
#   type,   # one of {header, credits, animation} (string)
#   data    # arbitrary data of block
# )
sub genblock {
    my ($type, $data) = @_;

    my $id;
    for ($type) {
        when ('header')     { $id = 0x01 }
        when ('credits')    { $id = 0x02 }
        when ('animation')  { $id = 0x03 }
        default             { die "Unknown type $type" }
    }

    return pack('CQa*', $id, (length $data), $data);
}

# usage: gencaff(
#   author,             # creator of image
#   framesRef,          # ref to an array of frame hashes described
#                         below
#   date                # creation date (DateTime)
# )
#
# Required fields of the frame hashes:
#   duration    : int   # frame duration in milliseconds
#   width       : int   # width of frame
#   height      : int   # height of frame
#   caption     : str   # caption of frame
#   tags        : str   # tags of frame (comma separated)
#   type        : str   # one of the types recognized by Ciff.pm
sub gencaff {
    my ($author, $framesRef, $date) = @_;
    my @frames = @$framesRef;

    my $hdrdata
      = pack 'A4Q2', MAGIC, HDRLEN, scalar @frames;
    my $hdr = genblock 'header', $hdrdata;

    my $creddata
      = pack 'SC4QA*',
        $date->year, $date->month, $date->day,
        $date->hour, $date->minute,
        length $author, $author;
    my $cred = genblock 'credits', $creddata;

    my @anims;
    for my $fr (@frames) {
        my $data
          = pack 'Qa*',
            $fr->{duration},
            genciff($fr->{width}, $fr->{height}, $fr->{caption},
              $fr->{tags}, $fr->{type});
        push @anims, genblock('animation', $data);
    }

    return $hdr . $cred . join('', @anims);
}


1;

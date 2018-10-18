#############################################################################
# Perl script symshift.pl --- shift symbols of different tables into proper 
#                             "plane" and create combined symbol table
# Copyright (C) 2009 SVOX AG. All Rights Reserved.
#
# type perl symshift.pl -help to get help
#
#############################################################################
# This script creates a symbol table which must be used when
# compiling a source FST into its binary format.
# 
# Explanation:
# When creating SVOX pico lingware, different sets of symbols (phonemes,
# Part-Of-Speech symbols, accents,boundaries) are expressed with
# names (strings) in the lingware source, but in the compiled lingware
# resources (bin-files) only ids (numbers) are used. 
# For each set, symbols are mapped into one-byte ids [0..255].
# Finite-State-Transducers are used to transform one sequence of symbols into
# another, where input and output symbols may be mixed from different sets.
# In order to keep the id ranges for each set disjoint, ids are shifted
# into a corresponding plane when forming such input sequences:
#
#    id_combined = id_original + 256 * plane
#
# Note: shifting/unshifting in the running system uses hard-coded
#       constants (e.g. the plane for each set). Also, some hard-coded
#       "universal" symbols are added that are not related to any particular
#       lingware but are inserted by the running system.
#       Therefore there is a hard dependency between this script and the
#       engine code!
#    
#
#############################################################################

eval "exec perl -S \$0 \${1+\"\$@\"}"
   if 0;
###################################################################
##
##  Imports
##
###################################################################
#use File::DosGlob 'glob';
#use File::Copy;
#use File::Path;
#use File::Basename;
#use Filehandle;
#use Time::Local;
use Getopt::Long;
###################################################################
##
##  Default values
##
###################################################################
$VALUE = 1;
$NAME = "name";
$DEST = ".";
###################################################################
##
##  Options
##
###################################################################
GetOptions(
          "phones=s" => \$PHONES,    # string
          "POS=s" => \$POS,    # string
          "accents=s" => \$ACCENTS,    # string
          "pb_strengths=s" => \$PB_STRENGTHS,    # string
          "alphabet=s" => \$ALPHABET,    # string
          "help"    => \$HELP
          );
###################################################################
##
##  Help
##
###################################################################
$help = <<EOHELP
   $0 -- 

Usage:
   $0 -help

      print this help

    $0  [-phones <phonestab>] [-POS <postab>] [-accents <acctab>]
         [-pb_strengths <pbstab>] [-alphabet <alphaout>]

    reads in a combination of symbol tables with ids in range [0..2^8-1]
    and converts into one symbol table with ids in range [0..2^16-1] which
    is written to STDOUT.

  (Read perl source for more explanations)

 Options:
    -phones <infile>,
    -POS <infile>,
    -accents <infile>,
    -pb_strengths <infile> read symbol tables from <file> and shift them into
                           the appropriate plane

                           A hard-coded universal set of accents and
                           pb_strengths is automatically included so that
                           usually only -phones ans -POS are used.

    -alphabet <outfile>    writes the combined set of symbols to <outfile>.
                           (Not used yet)

EOHELP
   ;
die $help if $HELP;

###################################################################
##
##  Initialization
##
###################################################################

@alltables = ("PHONES", "ACCENTS", "POS", "PB_STRENGTHS", "INTERN");

%plane = (
   PHONES => 0,
   ACCENTS => 4,
   POS => 5,
   PB_STRENGTHS => 6,
   INTERN => 7,
);

#sometimes we want the inverse
foreach $table (@alltables) {
	$table{$plane{$table}} = $table;
}


#translation between symbol names used in decision trees
#and corresponding names used in FSTs
%translation = (
#boundaries
    "PB_STRENGTHS" => {
	"0"         => "{WB}",
	"_SHORTBR_" => "{P2}",
	"_SECBND_"  => "{P3}",
    },
#accents
    "ACCENTS" => {
	"0" => "{A0}",
	"1" => "{A1}",
	"2" => "{A2}",
	"3" => "{A3}",
	"4" => "{A4}",
    },
);

# not all symbols are predicted by trees, some universals are inserted
# programatically. we add these hardcoded symbols/ids and check that they$
# don't collide with predicted ones
%notpredicted = (    
#boundaries
    "PB_STRENGTHS" => {
	"{WB}" => 48,
	"{P1}" => 49, 
	"{P2}" => 50, 
	"{P3}" => 51,  
	"{P0}" => 115,  # "s" 
    },
#accents
    "ACCENTS" => {
	"{A0}" => 48,
	"{A1}" => 49,
	"{A2}" => 50,
	"{A3}" => 51,
	"{A4}" => 52,
    },
#intern
    "INTERN" => {
	"&" => 38,
	"#" => 35,
	"|" => 50,
	"+" => 51,
	"*" => 52,
	"{DEL}" => 127,
    },
);



foreach $table (@alltables) {
    #printf STDERR "doing table $table (plane %d)\n", $plane{$table};
    $file = ${$table};
    if ($file) {
	$plane = $plane{$table};
	open TABLE, $file or die "can't open $table table $file";
	while (<TABLE>) {
	    #ignore empty lines
	    next if /^\s*$/;
	    #ignore comment lines
	    next if /^\s*[\!]/;
	    if (/^\s*:SYM\s+\"([^\"]+)\"(.*)$/) {
		($sym,$rest) = ($1,$2);
		#we have the symbol (which potentially contains an exclamation mark)
		#remove comments now
		$rest =~ s/[\!].*//;
		next if $rest =~ /iscombined/; #filter out combined POS
		if ($rest =~ /.*:PROP.*mapval\s*=\s*(\d+)/) {
		    $id = $1 + 0;
		    $shifted = $id + $plane * 256;
		    $sym = translate($table,$sym,$id);
		    if ($shifted{$sym}) {
			$otherplane = int($shifted{$sym} / 256);
			print STDERR "symbol \"$sym\" was allready assigned to plane of \"$table{$otherplane}\" ($otherplane); overwriting\n";
		    }
		    $shifted{$sym} = $shifted;
		    $sym{$shifted} = $sym;
		    $intable{$table}{$shifted}++; 
		} else {
		    print STDERR "strange line (no mapval) in $file: $_";
		}
	    } else {
		print STDERR "strange line (no SYM) in $file: $_";
	    }
	}
    }
}

#insert not predicted symbols
foreach $table (keys %notpredicted) {
    $plane = $plane{$table};
    foreach $sym (keys %{$notpredicted{$table}}) {
	$id = $notpredicted{$table}{$sym};
	$shifted = $id + $plane * 256;
	$shifted{$sym} = $shifted unless $shifted{$sym};
	$sym{$shifted} = $sym unless $sym{$shifted};
	$intable{$table}{$shifted}++; 
    }
}
	
#create combined table
foreach $plane (sort numerically keys %table) {
    $table = $table{$plane};
    print "\n! $table\n";
    foreach $shifted (sort numerically keys %{$intable{$table}}) {
	printf ":SYM %-20s   :PROP mapval = %5d\n", "\"$sym{$shifted}\"", $shifted;
    }
}

#create corresponding alphabet if demanded
if ($ALPHABET) {
    open OUT, ">$ALPHABET" or die "cant open $ALPHABET for writing";
    foreach $plane (sort numerically keys %table) {
	$table = $table{$plane};
	print OUT "\n! $table\n   ";
	$count=10;
	foreach $shifted (sort numerically keys %{$intable{$table}}) {
	    $sym = $sym{$shifted};
	    $sym =~ s/'/''/g;
	    if (!$count--) {
		$count = 10;
		print OUT "\n   ";
	    }
	    printf OUT " %s", "\'$sym{$shifted}\'";
	}
    }
    close OUT;
}

sub numerically {$a <=> $b}
	

sub translate($$$) {
    my ($table,$sym,$id) = @_;
    my $translated;
    my $otherid;
    if ($table eq "POS") {
	$translated = "{P:$sym}";
    } else {
	$translated = $translation{$table}{$sym};
	$translated = $sym unless $translated;
	if (($other = $notpredicted{$table}{$translated}) && ($other != $id)) {
	    die "inconsistent table $table: sym \"$sym\" has id=$id, but i expected $other";
	} 
    }
    return $translated;
}    

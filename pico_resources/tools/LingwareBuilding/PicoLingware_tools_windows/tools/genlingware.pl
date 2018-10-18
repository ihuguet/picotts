#############################################################################
# Perl script genlingware.pl --- composes a lingware resource from
#                                pico knowledge base binary files (pkb)
#                                according to given configuration
#
# Copyright (C) 2009 SVOX AG. All Rights Reserved.
#
# type perl genlingware.pl -help to get help
#
#############################################################################
eval "exec perl -S \$0 \${1+\"\$@\"}"
    if 0;

$resource_structure = <<EOSTRUCT


the resource file structure is as follows:
------------------------------
1. optional foreign header (4-byte aligned), externally added
------------------------------
A) Pico header (4-byte aligned)
 2. SVOX PICO header (signature)
 3. length of header (2 byte, excluding length itself)
 4. number of fields  (1 byte)
 5. header fields (space separated) (key/value pairs)
 6. filler (0-3)
------------------------------
B) length of content
7. length of the remaining content in 4-byte, excluding length itself
------------------------------
C) Index
 8. summary: number of kbs
 9. id names of kb (strings of max 15 chars plus closing space) 
 10. directory (kb id (1 byte), offset (4 bytes), size (4 bytes))
 11. filler (0-3)
------------------------------
D) knowledge bases
12. sequence of knowledge bases (byte arrays), each 4-byte aligned

all numbers are little endian
EOSTRUCT
    ;





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
    "value=i" => \$VALUE,  # numeric
    "name=s" => \$NAME,    # string
    "help"    => \$HELP
    );
###################################################################
##
##  Help
##
###################################################################
$help = <<EOHELP
    $0 -- composes a lingware resource from pico knowledge base
          binary files (pkb) according to given configuration
    
  Usage:
    $0 -help
    
    print this help
    
    $0  <config> <resource> 
    
    reads in configuration file <config> and creates resource <resource>
    
  Arguments:
    <config>   :  configuration file (input)
    <resource> :  platform-independent resource file (output)
    

  (For more details see the source of this script)
EOHELP
    ;
die $help if $HELP;

$config_example = <<EOCONFIGEXAMPLE

Example of a config:

-------------------------------------------------------------------
\# collection of de-DE textana knowledge bases

\# header fields:

NAME                    de-DE_ta_1.0.0.0-0-2
VERSION                 1.0.0.0-0-2
DATE                    2009-01-15
TIME                    17:00:00.000
CONTENT_TYPE            TEXTANA


\# pico knowledge bases:

TPP_MAIN                \"../pkb/de-DE/de-DE_kpr.pkb\"
TAB_GRAPHS              \"../pkb/de-DE/de-DE_ktab_graphs.pkb\"
...

-------------------------------------------------------------------

for all recognized pkb tags, see %picoknow_kb_id below

EOCONFIGEXAMPLE
;    


###################################################################
##
##  Initialization
##
###################################################################
$svoxheader = " (C) SVOX AG ";

%header_field = (
    "NAME"    => 1,
    "VERSION" => 2,
    "DATE"    => 3,
    "TIME"    => 4,
    "CONTENT_TYPE" => 5,
);

%picoknow_kb_id = (
    NULL         => 0,
#base
    TAB_GRAPHS   => 2,
    TAB_PHONES   => 3,
    TAB_POS      => 4,
  # FIXED_IDS     = 7,
#dbg
    DBG          => 8,

#textana
    TPP_MAIN     => 1,
    LEX_MAIN     => 9,
    DT_POSP      => 10,
    DT_POSD      => 11,
    DT_G2P       => 12,
    FST_WPHO_1   => 13,
    FST_WPHO_2   => 14,
    FST_WPHO_3   => 15,
    FST_WPHO_4   => 16,
    FST_WPHO_5   => 17,
    DT_PHR       => 18,
    DT_ACC       => 19,
    FST_SPHO_1   => 20,
    FST_SPHO_2   => 21,
    FST_SPHO_3   => 22,
    FST_SPHO_4   => 23,
    FST_SPHO_5   => 24,
    FST_XSPA_PARSE   => 25,
    FST_SVPA_PARSE   => 26,
    FST_XS2SVPA   => 27,
    
    FST_SPHO_6   => 28,
    FST_SPHO_7   => 29,
    FST_SPHO_8   => 30,
    FST_SPHO_9   => 31,
    FST_SPHO_10   => 32,
    
#siggen
    DT_DUR       => 34,
    DT_LFZ1      => 35,
    DT_LFZ2      => 36,
    DT_LFZ3      => 37,
    DT_LFZ4      => 38,
    DT_LFZ5      => 39,
    DT_MGC1      => 40,
    DT_MGC2      => 41,
    DT_MGC3      => 42,
    DT_MGC4      => 43,
    DT_MGC5      => 44,
    PDF_DUR      => 45,
    PDF_LFZ      => 46,
    PDF_MGC      => 47,
    PDF_PHS      => 48,

#user tpp
    TPP_USER1    => 49,
    TPP_USER2    => 50,
#user lex
    LEX_USER1    => 57,
    LEX_USER2    => 58,

    DUMMY => 127
    );




###################################################################
##
##  Get Parameters
##
###################################################################
($in,$out) = (shift,shift);

unless ($in && $out) {
    print "*** error: incorrect number of parameters\n"; 
    die $help;
}


###################################################################
##
##  Work
##
###################################################################

#get description of kbs

unless (open IN, $in) {
    print "*** error: can't open $in\n";
    die "can't open $in";
}
while (<IN>) {
    # skip comments
    next if /^\s*#/;    
    ($key, $value) = split;    
    next unless $key;
    $value =~ s/^\s*\"(.*)\"\s*$/\1/;
    if ($field = $header_field{$key}) {
        $fields[$field] = {key => "$key", value => "$value"}; 
        next;
    }      
    #print "$key -> $value\n";
    unless ($id = $picoknow_kb_id{$key}) {
	print "*** error: not a valid knowledge base name $key\n";
	die "not a valid knowledge base name $key" ;
    }
    push @kb, {name => $key, file => $value, id => $id};
} 
close IN;



#open output lingware file and write header

unless (open OUT, ">$out") {
    print "*** error: can't open $out for writing\n";
    die "can't open $out for writing";
}
binmode(OUT);

$offs = 0;

###################################################################
##
##  A) PICO HEADER
##
###################################################################
# 1. SVOX HEADER
foreach $ch (split //, $svoxheader) {
    push @svoxheader, chr(ord($ch)-ord(' '));
}
$offs += print_offs(@svoxheader);

print "offset after svoxheader: $offs\n";

# fields header
$fieldheader = "";
foreach $field (@fields) {
    $fieldheader .= $field->{key} .  " " . $field->{value} . " ";
}

#print size of fields header

# length of the fields plus preceding number of fields (1) 
$len = length($fieldheader)+1;

#fill should make the whole header 4-byte aligned, i.e. the current offs
# (svoxheader) plus the length of the header (2) plus the fields
$fill = ($offs + 2 + $len) % 4;
$fill = $fill ? 4-$fill : 0;
$len += $fill;

print "filled length of header : $len\n";

# 2. length of header
$offs += &print_uint16($len); #write little-endian 16-bit cardinal
print "offset after length of header: $offs\n";

# 3. print number of fields
$offs += &print_uint8(@fields+0);
print "offset after number of fields: $offs\n";

# 4. print fields
$offs += print_offs($fieldheader);
print "offset after fields: $offs\n";

# 5. print magic number (not yet)

# 6. print filler
$offs += &opt_fill($fill);
print "offset after fill: $offs\n";


###################################################################
##
##  CONTENT (that is actually saved in RAM)
##
###################################################################



# open kb files to get sizes and calculate fillers 

foreach $kb (@kb) {
    if ($kb->{file}) {
	unless (open IN, $kb->{file}) {
	    print "*** error: can't open " . $kb->{file} ."\n";
	    die("can't open PKB " . $kb->{file});
	}
	binmode(IN);
	#slurp in the whole file
	@content = <IN>;
	close IN;
	$kb->{size} = length(join '',@content);
	$fill = ($kb->{size} % 4);
	$kb->{fill} = $fill ? 4-$fill : 0;
	print "fill of ", $kb->{name} , " is ", $kb->{fill}+0, "\n";
	$totalcont += $kb->{size} + $kb->{fill};
    } else {
	$kb->{size} = 0;
    }
    print $kb->{name}, " -> ", $kb->{file}, " -> ", $kb->{size}, "\n";
}


# calculate total content size (B):

$totalcont = 0;

# size of number of kbs (7.)
$totalcont += 1;  

#size of names of kbs (8.)
foreach $kb (@kb) {
    $totalcont += &size_offs($kb->{name}, " ");
}


# size of directory (9.)

$totalcont += 9 * @kb;

# size of filler (10.)
$xfill = $totalcont % 4;
$xfill = $xfill ? 4 - $xfill : 0;
$totalcont += $xfill;

#set root of first kb
$offs1 = $totalcont;

print "totalcont before kbs: $totalcont\n";
# size of actual kbs
foreach $kb (@kb) {
    $totalcont += $kb->{size} + $kb->{fill};
}
print "totalcont after kbs: $totalcont\n";

# B) print the total size

$offs += &print_uint32($totalcont);

print "offset after size of content (to be loaded): $offs\n";

# here starts the part that is stored in "permament memory"

$offs = 0;

print "offset after reset: $offs\n";



# 7. print number of kbs:
$offs += &print_uint8(@kb+0);
print "offset after number of kbs: $offs\n";


# 8. print names of kbs
foreach $kb (@kb) {
    $offs += &print_offs($kb->{name}, " ");
}
print "offset after descriptive kb names: $offs\n";

# 9. print directory (ids and offsets of kbs)


print "first kb should start at $offs1\n";
foreach $kb (@kb) {
    $offs += &print_uint8($kb->{id});
    print "kb id: $kb->{id}, offs=$offs\n"; 
    if ($kb->{size}) {
    print "real kb (size $kb->{size})\n"; 
	$offs += &print_uint32($offs1);
    print "kb offs: $offs1, offs=$offs\n"; 
	$offs += &print_uint32($kb->{size});
    print "kb size: $kb->{size}, offs=$offs\n"; 
	$offs1 += $kb->{size} + $kb->{fill};
    } else {
    print "dummy kb (size 0)\n"; 
	$offs += &print_uint32(0);
    print "kb offs: 0, offs=$offs\n"; 
	$offs += &print_uint32(0);
    print "kb size: $kb->{size}, offs=$offs\n"; 
    }
    print "offset after directory entry for kb $kb->{name} (id $kb->{id}): $offs\n";
}

# 10. print filler
$offs += &opt_fill($xfill);
print "offset after fill: $offs\n";


# print kbs themselves

foreach $kb (@kb) {
    if ($kb->{file}) {
	unless (open IN, $kb->{file}) {
	    print "*** error: can't open " . $kb->{file} ."\n";
	    die "can't open " . $kb->{file};
	}
	binmode(IN);
	#slurp in the whole file
	@content = <IN>;
	close IN;
	print OUT join '', @content;
	$offs +=  $kb->{size};
	$offs += &opt_fill($kb->{fill});
	print "offset after kb $kb->{file}: $offs\n";
    } else {
    }
    #print $kb->{name}, " -> ", $kb->{file}, " -> ", $kb->{size}, "\n";
}


close OUT;



# optional filler
# use for alignment if filler doesn't need to be parsed 
sub opt_fill() {
    my ($fill) = @_;
    my $size = 0;
    if ($fill) {
	$size += &print_uint8($fill);
	for (my $i = 1; $i < $fill; $i++) {
	    $size += &print_uint8(0);
	}
    }
    return $size;
}

# mandatory filler
# use for alignment if filler needs to be parsed (at least one byte) 
sub mand_fill() {
    my ($fill) = @_;
    my $size = 0;
    #force fill to be 4 if there is no fill
    $fill = 4 unless $fill;
    $size += &print_uint8($fill);
    for (my $i = 1; $i < $fill; $i++) {
	$size += &print_uint8(0);
    }
    return $size;
}

sub print_offs() {
    my (@cont) = @_;
    my $size = 0;
    foreach my $cont (@cont) {
	$size += length($cont);
	print OUT $cont;
    }
    return $size;
}

sub size_offs() {
    my (@cont) = @_;
    my $size = 0;
    foreach my $cont (@cont) {
	$size += length($cont);
    }
    return $size;
}

sub print_uint_n() {
    #little-endian n-byte cardinal (no check, though!)
    my ($num, $n) = @_;
    my (@out) = ();
    for (my $i=0; $i < $n;$i++) {
	$out[$i] = $num % 256;
	$num = int($num / 256);
    }
    print OUT pack("c*",@out);
    return $n;
}

sub print_uint32() {
    #little-endian 4-byte cardinal (no check, though!)
    my ($num) = @_;
    return &print_uint_n($num,4);
}

sub print_uint16() {
    #little-endian 2-byte cardinal (no check, though!)
    my ($num) = @_;
    return &print_uint_n($num,2);
}
sub print_uint8() {
    my ($num) = @_;
    return &print_uint_n($num,1);
}

sub numerically {$x < $y}
    
# @x = map {chr(48+$_) } (0..9);
# foreach $x (@x) {
#     print $x, "\n";
# }
# @x = map +(chr(65+$_)), (0..9);
# foreach $x (@x) {
#     print $x, "\n";
# }

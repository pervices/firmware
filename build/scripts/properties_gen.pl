#!/usr/bin/perl
use strict;
use warnings;

############################################################################
# Property Table (possible options)
#
# POLL	| PERM	| GET		| SET
# ------|-------|---------------|-------
# N	| RW	| PWRON		| MCU/MEM
# Y	| RW	| -		| -
# N	| RO	| PWRON		| -
# Y	| RO	| MCU/MEM	| -
# N	| WO	| -		| MCU/MEM
# Y	| WO	| -		| -
#
#########

# Check for parameter to be passed to the script
die "Please pass the property description file to script!\n" unless $ARGV[0];

# Declaration of all the variables that are used for auto-generation
my @PROPERTIES_ARRAY;
my $i;

# Read in the file name
my $INPUT_FILE = $ARGV[0];
my $OUTPUT_FILE = "output.c";

############################################################################
# Description File Parsing
############################################################################
# Open the file and read in all the variables
open(my $fh, '<:encoding(UTF-8)', $INPUT_FILE)
  or die "Could not open file '$INPUT_FILE' $!";

while (my $row = <$fh>) {
  	chomp $row;
	if ($row eq "") {
		next;
	}
	$row =~ /(.*) (.*) (.*) (.*)/;

	my $poll = 0;
	if ($1 eq 'y') {
		$poll = "POLL";
	} else {
		$poll = "NO_POLL";
	}
	my $perm = uc $2;

	if ($poll eq "POLL" && $perm eq "RW") {
		printf "invalid property combination, POLL and RW found\n";
		exit;
	}

	if ($poll eq "POLL" && $perm eq "WO") {
		printf "invalid property combination, POLL and RW found\n";
		exit;
	}

	my @temp = ($3, $3, $poll, $perm, $4);
	$temp[1] =~ tr/\//_/;
	push(@PROPERTIES_ARRAY, \@temp);
}

# close the file
close($fh);

############################################################################
# Generate the output
############################################################################
# open and generate the output file
open($fh, '>', $OUTPUT_FILE)
  or die "Could not open file '$OUTPUT_FILE' $!";

print $fh "// Beginning of property functions, very long because each property needs to be\n";
print $fh "// handled explicitly\n";

# Invalid filler functions
print $fh "static int get_invalid (const char* data) {";
print $fh "\n\tprintf(\"Cannot invoke a get on this property\\n\");";
print $fh "\n\treturn RETURN_ERROR_GET_PROP;";
print $fh "\n}";
print $fh "\n";
print $fh "\nstatic int set_invalid (const char* data) {";
print $fh "\n\tprintf(\"Cannot invoke a set on this property\\n\");";
print $fh "\n\treturn RETURN_ERROR_SET_PROP;";
print $fh "\n}";
print $fh "\n";

for  ($i = 0; $i < @PROPERTIES_ARRAY; $i++) {
	# Get function
	if ($PROPERTIES_ARRAY[$i][2] eq "NO_POLL") {

	} else {
		print $fh "static int get_$PROPERTIES_ARRAY[$i][1] (const char* data) {\n";
		if ($PROPERTIES_ARRAY[$i][2] eq "POLL") {
			print $fh "\t// Insert MCU/MEM command\n";
		} else {
			print $fh "\t// Insert PWR ON settings\n";
		}
		print $fh "\n\treturn RETURN_SUCCESS;\n}\n\n";
	}

	# Set function
	if ($PROPERTIES_ARRAY[$i][3] eq "RO") {

	} else {
		print $fh "static int set_$PROPERTIES_ARRAY[$i][1] (const char* data) {\n";
		print $fh "\t// Insert MCU/MEM command\n";
		print $fh "\n\treturn RETURN_SUCCESS;\n}\n\n";
	}
}

print $fh "// Beginning of property table\n";
print $fh "static prop_t property_table[] = {\n";
for  ($i = 0; $i < @PROPERTIES_ARRAY; $i++) {
	print $fh "\t{\"$PROPERTIES_ARRAY[$i][0]\",";

	if ($PROPERTIES_ARRAY[$i][3] eq "RO" && $PROPERTIES_ARRAY[$i][2] eq "NO_POLL") {
		print $fh " get_invalid, set_invalid, ";
	} elsif ($PROPERTIES_ARRAY[$i][3] eq "RO") {
		print $fh " get_$PROPERTIES_ARRAY[$i][1], set_invalid, ";
	} elsif ($PROPERTIES_ARRAY[$i][2] eq "NO_POLL") {
		print $fh " get_invalid, set_$PROPERTIES_ARRAY[$i][1], ";
	} else {
		print $fh " get_$PROPERTIES_ARRAY[$i][1], set_$PROPERTIES_ARRAY[$i][1], ";
	}

	print $fh "$PROPERTIES_ARRAY[$i][3], $PROPERTIES_ARRAY[$i][2], \"$PROPERTIES_ARRAY[$i][4]\"}";
	if ($i != @PROPERTIES_ARRAY - 1) {
		print $fh ",\n";
	} else {
		print $fh "\n";
	}
}
printf $fh "};\n";
printf $fh "static size_t num_properties = sizeof(property_table) / sizeof(property_table[0]);\n";


close($fh);

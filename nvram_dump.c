// nvram_dump.c
// Copyright 2015, Todd Knarr <tknarr@silverglass.org>
// Licensed under the terms of the GPL v3 or any later version.
// See LICENSE.md for complete license terms.

//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Simple program to read a DD-WRT NVRAM backup file and convert it into
// name=value pairs in a text file. Control characters and non-ASCII
// characters are escaped using standard C escape sequences and hex
// notation. If '-h' is given as an option newline characters are output
// as a backslash followed by a newline, allowing human-readable output
// that shows line breaks. Otherwise newlines are escaped and each
// entry will occupy one and only one line in the file. Names are
// always fully escaped since we expect them to never contain newlines.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define ESC_FULL   0
#define ESC_HUMAN  1

// Returns the number of characters copied to dest.
int escape_string( int escape_mode, const char *src, char *dest, int max )
{
	if ( !src || !dest || max <= 0 )
		return 0;

	char tmpbuf[8]; // Long enough for longest single escape sequence

	int i, j = 0, len = strlen( src );
	for ( i = 0; i < len; i++ )
	{
		if ( isascii( src[i] ) )
		{
			if ( iscntrl( src[i] ) )
			{
				if ( src[i] == '\n' )
				{
					if ( escape_mode == ESC_HUMAN )
						strcpy( tmpbuf, "\\\n" );
					else
						strcpy( tmpbuf, "\\n" );
				}
				else if ( src[i] == '\a' )
					strcpy( tmpbuf, "\\a" );
				else if ( src[i] == '\b' )
					strcpy( tmpbuf, "\\b" );
				else if ( src[i] == '\f' )
					strcpy( tmpbuf, "\\f" );
				else if ( src[i] == '\r' )
					strcpy( tmpbuf, "\\r" );
				else if ( src[i] == '\t' )
					strcpy( tmpbuf, "\\t" );
				else if ( src[i] == '\v' )
					strcpy( tmpbuf, "\\v" );
				else
					sprintf( tmpbuf, "\\x%02.2X", (unsigned int) ( src[i] & 0xFF ) );
			}
			else if ( src[i] == '\\' )
				strcpy( tmpbuf, "\\\\" );
			else
			{
				tmpbuf[0] = src[i];
				tmpbuf[1] = '\0';
			}
		}
		else
			sprintf( tmpbuf, "\\x%02.2X", (unsigned int) ( src[i] & 0xFF ) );

		if ( j + strlen( tmpbuf ) >= max )
			break;
		strcpy( dest+j, tmpbuf );
		j += strlen( tmpbuf );
	}

	return i;
}

int dump_file( int escape_mode, const char *filename )
{
	if ( !filename || ( strlen( filename ) == 0 ) )
	{
		fprintf( stderr, "dump_file: No filename given\n" );
		return 1;
	}
	
	FILE *f = fopen( filename, "rb" );
	if ( !f )
	{
		int code = errno;
		char *errstr = strerror( code );
		fprintf( stderr, "dump_file: Error opening %s: %s\n", filename, errstr );
		return 1;
	}

	static unsigned char buffer[65536]; // Static buffer for reading from nvram backup file
	size_t read_bytes;

	read_bytes = fread( buffer, sizeof (char), 8, f );
	if ( read_bytes != 8 || memcmp( buffer, "DD-WRT", 6 ) )
	{
		fprintf( stderr, "dump_file: File %s: Error reading header and record count\n", filename );
		fclose( f );
		return 1;
	}
	unsigned int record_count = buffer[7] * 256 + buffer[6]; // TODO byte ordering
	unsigned int name_len, value_len;
	static char name[257], value[65537];
	unsigned int record = 0;

	while ( record < record_count )
	{
		record++;

		// Read the 1-byte length and the variable name.
		read_bytes = fread( buffer, 1, 1, f );
		if ( read_bytes != 1 )
		{
			fprintf( stderr, "dump_file: File %s: Error reading name length from record %u\n",
					 filename, record );
			fclose( f );
			return 1;
		}
		name_len = buffer[0];
		read_bytes = fread( buffer, 1, name_len, f );
		if ( read_bytes != name_len )
		{
			fprintf( stderr, "dump_file: File %s: Error reading name from record %u\n",
					 filename, record );
			fclose( f );
			return 1;
		}
		memcpy( name, buffer, name_len );
		name[name_len] = 0;

		// Read the 2-byte length and the value.
		read_bytes = fread( buffer, 1, 2, f );
		if ( read_bytes != 2 )
		{
			fprintf( stderr, "dump_file: File %s: Error reading value length from record %u\n",
					 filename, record );
			fclose( f );
			return 1;
		}
		value_len = buffer[1] * 256 + buffer[0]; // TODO byte ordering
		if ( value_len > 0 )
		{
			read_bytes = fread( buffer, 1, value_len, f );
			if ( read_bytes != value_len )
			{
				fprintf( stderr, "dump_file: File %s: Error reading value from record %u\n",
						 filename, record );
				fclose( f );
				return 1;
			}
			memcpy( value, buffer, value_len );
			value[value_len] = 0;
		}
		else
		{
			value[0] = 0;
		}

		static char esc_name[513], esc_value[65536*2 + 1];
		int copied;

		esc_name[0] = 0;
		esc_value[0] = 0;
		copied = escape_string( ESC_FULL, name, esc_name, 513 );
		if ( copied < strlen( name ) )
			fprintf( stderr, "dump_file: File %s: Record %u: cannot copy entire name %s\n",
					 filename, record, name );
		else if ( strlen( name ) < strlen( esc_name ) )
			fprintf( stderr, "dump_file: File %s: Record %u: Name %s: contains non-printable characters\n",
					 filename, record, esc_name );
		copied = escape_string( escape_mode, value, esc_value, 65536*2 + 1 );
		if ( copied < strlen( value ) )
			fprintf( stderr, "dump_file: File %s: Record %u: Name %s: cannot copy entire value\n",
					 filename, record, esc_name );
		fprintf( stdout, "%s=%s\n", esc_name, esc_value );
		fflush( stdout );
	}

	fclose( f );
	return 0;
}

int main( int argc, char **argv )
{
	int escape = ESC_FULL;
	
	// Check our arguments for options, and for at least one filename after
	// the options.
	int opt;
	while ( ( opt = getopt( argc, argv, "h" ) ) != -1 )
	{
		switch ( (char) opt )
		{
		case 'h':
			escape = ESC_HUMAN;
			break;

		default:
			fprintf( stderr, "Usage: %s [-h] <filename>...\n", argv[0] );
			return 1;
		}
	}
	if ( optind >= argc )
	{
		fprintf( stderr, "Expected at least one file\n" );
		fprintf( stderr, "Usage: %s [-h] <filename>...\n", argv[0] );
		return 1;
	}

	// Dump out each filename given. If any file fails, we fail.
	int sts, i;
	int ret = 0;
	for ( i = optind; i < argc; i++ )
	{
		if ( argv[i] )
		{
			sts = dump_file( escape, argv[i] );
			// Remember our first failure, but keep on going with the rest of the
			// files so we catch all errors in one pass.
			if ( sts && !ret )
				ret = sts;
		}
	}
	return ret;
}

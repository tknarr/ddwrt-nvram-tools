// nvram_build.c
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

// Simple program to read a file created by nvram_dump and convert it back
// into a DD-WRT NVRAM backup file. Any escaped characters in the input
// files are unescaped before writing. Both the normal form and the
// human-readable form with line breaks can be handled.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

int unescape_string( const char *src, char *dest )
{
	const char *p = src;
	char *q = dest;
	while ( *p )
	{
		if ( *p == '\\' )
		{
			p++;
			switch ( *p )
			{
			case 'a':
				*q = '\a';
				p++; q++;
				break;
			case 'b':
				*q = '\b';
				p++; q++;
				break;
			case 'f':
				*q = '\f';
				p++; q++;
				break;
			case 'n':
				*q = '\n';
				p++; q++;
				break;
			case 'r':
				*q = '\r';
				p++; q++;
				break;
			case 't':
				*q = '\t';
				p++; q++;
				break;
			case 'v':
				*q = '\v';
				p++; q++;
				break;
			case '\\':
				*q = '\\';
				p++; q++;
				break;

			case 'x':
				{
					p++;
					char s[3];
					s[0] = *p;
					s[1] = *(p+1);
					s[2] = 0;
					char *eptr = NULL;
					errno = 0;
					long v = strtol( s, &eptr, 16 );
					if ( eptr != s+2 || errno != 0 )
						return 1;
					*q = (char) v;
					p += 2;
				}
				q++;
				break;

			default:
				*q = *p;
				p++; q++;
				break;
			}
		}
		else
		{
			*q = *p;
			p++; q++;
		}
	}
	*q = 0;
	return 0;
}

// Returns the number of records written, or -1 if an error occurred.
int build_file( FILE *output_file, const char *filename )
{
	if ( !output_file )
	{
		fprintf( stderr, "build_file: No output file given\n" );
		return -1;
	}
	if ( !filename || ( strlen( filename ) == 0 ) )
	{
		fprintf( stderr, "build_file: No input file given\n" );
		return -1;
	}
	
	FILE *f = fopen( filename, "rb" );
	if ( !f )
	{
		int code = errno;
		char *errstr = strerror( code );
		fprintf( stderr, "build_file: Error opening %s for input: %s\n", filename, errstr );
		return -1;
	}
	// Brute-force approach here. Most routers won't have more than 64K or maybe 128K of NVRAM,
	// so I'm just going to make a 128K static buffer and read the whole file in and then parse
	// it in memory. That should be sufficient since most backups won't be of a completely-full
	// NVRAM image. If the input file won't fit, it'll output an error. Not elegant, but a lot
	// easier to code than trying to read chunks from a file and deal with split lines and such.
	static char buffer[128*1024 + 1];
	size_t bytes_read = fread( buffer, sizeof (char), 128*1024+1, f );
	if ( bytes_read <= ( 128*1024 ) )
	{
		// Got a complete file
		buffer[128*1024] = 0;
	}
	else
	{
		// File too large or some other error
		fprintf( stderr, "build_file: Problem reading %s\n", filename );
		fclose( f );
		return -1;
	}
	fclose( f );

	// Human-readable newlines are a backslash followed by a newline, which is
	// backslash followed by 'n' in fully-escaped form. So run through the buffer
	// and make that substitution to avoid complicated code for splicing together
	// multiple lines.
	int i, max = strlen( buffer );
	for ( i = 0; i < max; i++ )
	{
		if ( buffer[i] == '\\' && buffer[i+1] == '\n' )
			buffer[i+1] = 'n';
	}

	// Parse lines out of the buffer and output them as parameter records, counting
	// records as we go.
	static char output_buffer[65536+256+4]; // Build output record here
	static char r_name[256+1], r_value[65536+1]; // Buffers for unescaping the name and value
	int record_count = 0, line_number = 0;
	char *p_start = buffer, *p_end = buffer + strlen( buffer );
	while ( p_start < p_end )
	{
		line_number++;
		char *p_equals = strchr( p_start, '=' );
		char *p_newline = strchr( p_start, '\n' );
		if ( !p_newline )
			p_newline = p_start + strlen( p_start ); // Last line lacks a newline character
		if ( !p_equals )
		{
			// Error, no equals sign on the line
			fprintf( stderr, "build_file: Line %d: missing equals sign\n", filename, line_number );
			p_start = p_newline + 1;
			continue;
		}
		// Terminate our parts to give us usable strings.
		*p_newline = 0;
		*p_equals = 0;
		// Convenient names for our name and value strings.
		char *name = p_start;
		char *value = p_equals + 1;
		// And set up past the newline for the next iteration.
		p_start = p_newline + 1;
		// Sanity checks.
		if ( strlen( name ) == 0 )
		{
			fprintf( stderr, "build_file: Line %d: name is empty\n", filename, line_number );
			continue;
		}
		// Unescape our name and value.
		int sts;
		sts = unescape_string( name, r_name );
		if ( sts != 0 )
		{
			fprintf( stderr, "build_file: Line %d: problem unescaping name\n",
					 filename, line_number );
			continue;
		}
		sts = unescape_string( value, r_value );
		if ( sts != 0 )
		{
			fprintf( stderr, "build_file: Line %d: problem unescaping value\n",
					 filename, line_number );
			continue;
		}
		// And use the unescaped forms after this
		name = r_name;
		value = r_value;
		
		// Now to convert the name and value into a record.
		int record_len = 0;
		int len = strlen( name ) & 0xFF; // Only 1 byte for the name length
		output_buffer[0] = len;
		strncpy( output_buffer+1, name, len );
		record_len += len + 1; // Name length plus name
		int vstart = len+1;
		len = strlen( value ) & 0xFFFF; // Only 2 bytes for the value length
		output_buffer[vstart] = len & 0xFF; // TODO byte ordering
		output_buffer[vstart+1] = ( len >> 8 ) & 0xFF;
		vstart += 2;
		strncpy( output_buffer+vstart, value, len );
		record_len += len + 2; // Value length plus value
		// And write out our record and count it (we only want to count records we wrote).
		size_t bytes_written = fwrite( output_buffer, sizeof (char), record_len, output_file );
		if ( bytes_written != record_len )
		{
			fprintf( stderr, "build_file: Line %d: error writing record %d\n",
					 filename, line_number, record_count+1 );
			return -1;
		}
		record_count++;
	}

	return record_count;
}

int output_header( FILE *output_file )
{
	if ( !output_file )
	{
		fprintf( stderr, "output_header: No output file given\n" );
		return 1;
	}

	// Put 2 zero bytes in the header, we'll set them to the number of records at the end.
	size_t bytes_written = fwrite( "DD-WRT\0\0", sizeof (char), 8, output_file );
	if ( bytes_written < 8 )
	{
		fprintf( stderr, "output_header: Problem writing header\n" );
		return 1;
	}
	return 0;
}

int fixup_record_count( FILE *output_file, int record_count )
{
	if ( !output_file )
	{
		fprintf( stderr, "fixup_record_count: No output file given\n" );
		return 1;
	}

	int bytes_written, sts;
	unsigned char buffer[2];

	// Rewind file and position at the record count.
	sts = fseek( output_file, 6, SEEK_SET );
	if ( sts != 0 )
	{
		fprintf( stderr, "fixup_record_count: Error repositioning to update record count\n" );
		return 1;
	}

	// Format and write the record count into the file.
	buffer[0] = record_count & 0xFF; // TODO byte ordering
	buffer[1] = ( record_count >> 8 ) & 0xFF;
	bytes_written = fwrite( buffer, sizeof (char), 2, output_file );
	if ( bytes_written != 2 )
	{
		fprintf( stderr, "fixup_record_count: Error writing record count\n" );
		return 1;
	}
	return 0;
}

int main( int argc, char **argv )
{
	// If no -o option is given, we default to the base name of the first
	// input file plus ".bin".
	char output_filename[65541]; // Length is 64K for string + 4 for possible extention + 1 for terminating NUL

	memset( output_filename, 0, 65541 );
	
	// Check our arguments for options, and for at least one filename after
	// the options.
	int opt;
	while ( ( opt = getopt( argc, argv, "o:" ) ) != -1 )
	{
		switch ( (char) opt )
		{
		case 'o':
			strncpy( output_filename, optarg, 65536 );
			output_filename[65536] = 0;
			break;

		default:
			fprintf( stderr, "Usage: %s [-o <output_filename>] <filename>...\n", argv[0] );
			return 1;
		}
	}
	if ( optind >= argc )
	{
		fprintf( stderr, "Expected at least one input file\n" );
		fprintf( stderr, "Usage: %s [-o <output_filename>] <filename>...\n", argv[0] );
		return 1;
	}

	int i;

	// If we weren't given an output filename, find the first input file and
	// we'll use it's name as a base for an output filename.
	if ( strlen( output_filename ) == 0 )
	{
		for ( i = optind; i < argc; i++ )
		{
			if ( argv[i] )
			{
				strncpy( output_filename, argv[i], 65536 );
				output_filename[65536] = 0;
				break;
			}
		}

		// Change the filename's extension to ".bin", or add ".bin" if
		// the file didn't have an extension.
		char *p_dot, *p_slash;
		p_dot = strrchr( output_filename, '.' );
		p_slash = strrchr( output_filename, '/' );
		// If we found a dot and either there isn't any slash or the dot occurs
		// after the slash, we have an extention to replace. Otherwise, we have
		// no extension and just append our own.
		if ( p_dot && ( !p_slash || ( p_dot > p_slash ) ) )
		{
			// We have an extension in the trailing path segment, replace it with ".bin".
			// Due to our extra length we will always have room for 4 extra characters.
			strcpy( p_dot, ".bin" );
		}
		else
		{
			// No extension seen in the trailing path segment, append ".bin".
			// Due to our extra length we will always have room for 4 extra characters.
			strcat( output_filename, ".bin" );
		}
	}

	// Build output from files given. If any file fails, we fail.
	FILE *f = NULL;
	int record_count = 0;
	int ret = 0, sts;
	for ( i = optind; i < argc; i++ )
	{
		if ( argv[i] )
		{
			// Open the file the first time we have something to output.
			if ( !f )
			{
				f = fopen( output_filename, "wb" );
				if ( !f )
				{
					int code = errno;
					char *errstr = strerror( code );
					fprintf( stderr, "main: Error opening %s for output: %s\n", output_filename, errstr );
					return 1;
				}
				sts = output_header( f );
				if ( sts != 0 )
				{
					ret = 1;
					break;
				}
			}

			int cnt;
			cnt = build_file( f, argv[i] );
			if ( cnt < 0 )
				ret = 1;
			else
				record_count += cnt;
		}
	}
	if ( f )
	{
		if ( ret == 0 )
		{
			sts = fixup_record_count( f, record_count );
			if ( sts != 0 )
			{
				fprintf( stderr, "main: Error updating final record count\n" );
				ret = 1;
			}
		}
		fclose( f );
	}
	return ret;
}

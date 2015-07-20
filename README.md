# nvram_dump and nvram_build
## Todd Knarr, 2015
### tknarr@silverglass.org
### http://technical.silverglass.org/
### https://plus.google.com/u/0/+ToddKnarr/

Two tools I wrote to help with dealing with DD-WRT NVRAM backup files. I wound
up needing to be able to dump them out in a human-readable form for my records
and so I could see if there were any problematic entries, and when I found
some problematic entries I wanted to be able to rebuild the backup file after
editing them out.


#### nvram_dump

nvram_dump handles turning the backup into a text file of "name=value" entries
similar to what you get from the "nvram show" command. The big difference is
that non-printable characters are escaped using backslashes and standard C
escape sequences. I don't use the complete set, backslash itself is the only
printable character that's escaped because quotation marks and such aren't
special the way they are in C and it's more readable if they're just left
alone. The command looks like:

        nvram_dump [-h] filename ...

with one or more backup files listed on the command line. It writes the output
on the console, or you can redirect it to whatever file you want. If multiple
input files are given on the command line, it'll just output them all as a
single stream.

The -h switch changes entries with multi-line values (eg. SSH keys) to a form
that's easier to read for humans. Normally newlines are encoded as '\n' and
each entry occupies one physical line in the file. With -h newlines are
encoded as '\' at the end of the line followed by a newline and the next line
of the value begins on the next line of the file, so that the output retains
the line breaks of the original. The last line of the value won't have a '\'
at the end, or if it did in the data it'll have one escaped as a '\\' double
backslash, so you can tell where the value ends and the next entry line
begins.

Diagnostic messages are written to the standard error stream. The program
exits with a 0 exit code if everything went well and 1 if an error occurred.
There are some messages that aren't considered errors, like ones complaining
of non-printable characters in entry names.

##### Examples:

    nvram_dump nvram.bin >nvram.txt
Reads nvram.bin and produces nvram.txt

    nvram_dump -h nvram1.bin nvram2.bin nvram3.bin >nvram.txt
Reads 3 backup files and produces nvram.txt with line breaks in values preserved


#### nvram_build

nvram_build reverses what nvram_dump does, taking a file of "name=value"
entries and turning them back into a valid DD-WRT NVRAM backup file. It
handles both of the ways of handling newlines in values that nvram_dump does,
so you can send any nvram_dump output back through nvram_build to recreate the
backup. The command looks like:

    nvram_build [-o output_filename] filename...

with one or more input files listed on the command line. If you don't use the
-o switch the program takes the first input filename and replaces any
extension it has with ".bin" (or adds a ".bin" extension if the filename
didn't have an extension) and uses that as the name of the backup file that'll
be output. It keeps any path you used, so the output will end up in the same
directory as the first input file. You can use the -o switch to override this
and specify a filename for the resulting backup file.

Diagnostic messages are written to the standard error stream. The program
exits with a 0 exit code if everything went well and 1 if an error occurred.

##### Examples:

    nvram_build nvram.txt
Reads nvram.txt and builds nvram.bin

    nvram_build /tmp/router.backup
Reads /tmp/router.backup and builds /tmp/router.bin

    nvram_build -o new.bin nvram1.txt nvram2.txt
Reads two text files and builds new.bin


#### References:
- http://en.cppreference.com/w/cpp/language/escape - C escape sequences
- NvramBackupFormat.txt - internal format of the backup files

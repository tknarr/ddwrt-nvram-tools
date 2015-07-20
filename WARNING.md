# Byte ordering

The backup files for the routers I have all have the least significant byte
first for the 2-byte integers in the data. That's most likely because all of
them use similar CPUs, and the data's actually in the native byte order of the
CPU. These programs may need modified so you can tell them the byte
ordering. I've marked the places in the code where that needs to happen with
TODO markers.


# Static buffers

There's several static buffers in the program that're based on the assumption
that single values don't exceed 64K and NVRAM images 128K. That's probably
true for current routers, and it makes the code a lot simpler to just allocate
fixed-length buffers in the source code. I should probably go back and pull
those assumptions into a header file and fix the code to use symbols and
macros.


# Support

The DD-WRT team will most likely consider these programs unofficial and
unsupported. If you use them to build backup files and load those files into a
router, you're on your own. The same is true for me, the DD-WRT team may
change the backup format or the process of loading the backup into NVRAM
without warning causing files produced by nvram_build to suddenly start doing
anything from failing to load on up to bricking the router. Use caution when
using these tools to edit backups and reload them into the router.

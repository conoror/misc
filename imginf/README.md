# imginf

This dumps out jpeg and png image information in either a summary
basis or a detailed basis. Currently I've been too lazy to port
this properly to Unix so it sort of only works on Windows.

pnginf is pretty brief but jpginf dumps out lots of exif information if it's
there.

Compile with:

gcc -Wall -O2 -mconsole -o imginf.exe imginf.c pnginf.c jpginf.c ggetopt.c


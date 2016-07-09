# misc
Miscellaneous and utility code

clz is my own version of inflate (the gzip and pkzip decompression algorithm) using RFC 1951 as a basis. I found this hard enough to consider tackling deflate, the compression part, to be not worth the trouble! Clz uses callbacks to put and get data so it should be fairly flexible.

imginf dumps out png and jpg information including Exif and GPS information if present. A summary mode lists resolution and dpi for each file. It's not quite done as, because it started out as a quick test program, it grew whopping global-variable-reuse bugs...

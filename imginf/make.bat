@echo off
setlocal

set gccinps=imginf.c ggetopt.c jpginf.c exif.c pnginf.c
set gccopts=-Wall -mconsole -O2
set gccdefs=-DWINVER=0x0500 -D_WIN32_WINNT=0x500
set gcclibs=
set gccexec=imginf.exe

echo Compiling %gccexec%

@echo on
gcc %gccdefs% %gccopts% -o %gccexec% %gccinps% %gcclibs%

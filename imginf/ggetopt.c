/*
 *  ggetopt - Gumbo getopt()
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */

#include <string.h>
#include "ggetopt.h"

/* Global exported variables. I use capital O because reasons */

int   Optind = 1;       /* next index of argv[] to be evaluated     */
char *Optarg = 0;       /* pointer to any option argument           */
int   Optopt = '?';     /* If ? returned this is the problem option */


int gumbo_getopt(int argc, char *argv[], const char *optstring)
{
    static char *nextchar = 0;      /* Keep scan place inside argv */
    char *argmatch;
    int ch_err = '?';               /* Return char if an error     */

    Optarg = 0;
    Optopt = '?';

    if (*optstring == ':')
    {
        optstring++;
        ch_err = ':';
    }

    if (nextchar)
    {
        /* resume scan of current argv */
        nextchar++;
        if (!*nextchar)
            nextchar = 0;
    }

    if (!nextchar)
    {
        /* At the start of a new arg given by Optind */

        if (Optind >= argc)
            return -1;      /* Done - all used up */

        nextchar = argv[Optind++];

#ifdef _WIN32
        if (*nextchar != '/' && *nextchar != '-')
#else
        if (*nextchar != '-')
#endif
        {
            /* Not an option */
            Optind--;
            nextchar = 0;
            return -1;
        }
        else if (strcmp(nextchar, "-") == 0 || strcmp(nextchar, "--") == 0)
        {
            /* End of options */
            nextchar = 0;
            return -1;
        }

        nextchar++;   /* Hop over option character */
    }


    if (*nextchar == ':')
    {
        /* Can't have : as an option character */
        Optopt = ':';
        return ch_err;
    }

    argmatch = strchr(optstring, *nextchar);
    if (!argmatch)
    {
        /* No match found in optstring */
        Optopt = *nextchar;
        return ch_err;
    }

    /* Match found (char is also *argmatch) */

    if (argmatch[1] != ':')
    {
        /* A basic option with no argument */
        return *nextchar;
    }

    /* Argument must follow either in this argument ... */

    if (nextchar[1])
    {
        Optarg = nextchar + 1;
        nextchar = 0;
        return *argmatch;
    }

    /* ... or the next argument */

    if (Optind < argc)
    {
        Optarg = argv[Optind++];
        nextchar = 0;
        return *argmatch;
    }

    /* Um. Out of arguments. That's an error */

    Optopt = *argmatch;
    return ch_err;
}



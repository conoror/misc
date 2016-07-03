/*
 *  imginf - Dump out some image information from png and jpg files
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ggetopt.h"
#include "pnginf.h"
#include "jpginf.h"


int _CRT_glob = 0;  /* Mingw glob expands argv[] by default */


static int g_verbose;


void status_line(const char *fname, int status)
{
    switch (status)
    {
        case 1:  status = 'F'; break;
        case 2:  status = 'E'; break;
        default: status = 'C';
    }

    printf("%c%48s", status, " ");

    if (strlen(fname) >= 30)
        printf("%.26s...\n", fname);
    else
        printf("%s\n", fname);
}




void status_header(void)
{
    printf("   width  height  depth  colour  dpi  print(mm)  filename\n"
           "---------------------------------------------------------"
           "----------------------\n");
}




void process_img_all(void)
{
    WIN32_FIND_DATA finddata;
    HANDLE hFind;
    int len, ret, nfiles = 0;
    char *ext;

    hFind = FindFirstFile("*", &finddata);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        printf("No files found\n");
        return;
    }

    do {
        if ( (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
             (finddata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
             (finddata.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) )
            continue;

        len = strlen(finddata.cFileName);
        if (len < 5)
            continue;

        if (len >= 5 && finddata.cFileName[len - 4] == '.')
            ext = &finddata.cFileName[len - 3];
        else if (len >= 6 && finddata.cFileName[len - 5] == '.')
            ext = &finddata.cFileName[len - 4];
        else
            continue;

        if (strcasecmp(ext, "png") == 0)
        {
            if (nfiles++ == 0 && !g_verbose)
                status_header();

            ret = process_image_png(finddata.cFileName, g_verbose);
            if (!g_verbose && ret)
                status_line(finddata.cFileName, ret);
        }
        else if (strcasecmp(ext, "jpg") == 0 ||
                 strcasecmp(ext, "jpeg") == 0)
        {
            if (nfiles++ == 0 && !g_verbose)
                status_header();

            ret = process_image_jpg(finddata.cFileName, g_verbose);
            if (!g_verbose && ret)
                status_line(finddata.cFileName, ret);
        }

    } while (FindNextFile(hFind, &finddata));

    FindClose(hFind);

    if (nfiles == 0)
        printf("No files found\n");
}




void imginf_help(void)
{
    printf( "\n"
            "imginf usage:\n"
            "   imginf [-v] [file1] [file2] ...\n"
            "\n"
            "   With no files given, imginf scans the current directory\n"
            "   Files given cannot be wildcards or directories\n"
            "\n");
}




/*
 *  file_seems_valid(fname) - Quick check for file validity
 *
 *  Checks to see if fname does not include silly characters and
 *  ends with .png, .jpeg or .jpg. No other stat checks are done.
 *
 *  Returns: 0 if not valid
 *           1 if looks like a .png file
 *           2 if looks like a .jpg file
 */

int file_seems_valid(const char *fname)
{
    const char *s;
    const char *ldot = 0;

    if (!fname || !*fname || *fname == ' ')
        return 0;

    for (s = fname; *s; s++)
    {
        if (*s < 32 || *s == 127)
            return 0;

        if (strchr("*?\"<>|", *s))
            return 0;

        if (*s == '.')
            ldot = s;
    }

    if (!ldot)
        return 0;

    if (strcasecmp(ldot, ".png") == 0)
        return 1;

    else if (strcasecmp(ldot, ".jpeg") == 0)
        return 2;

    else if (strcasecmp(ldot, ".jpg") == 0)
        return 2;

    return 0;
}




int main(int argc, char **argv)
{
    int i, opt, ret;

    while ((opt = gumbo_getopt(argc, argv, ":h?v")) != -1)
    {
        switch (opt)
        {
            case '?':
            case 'h':
                imginf_help();
                return 1;

            case 'v':
                g_verbose = 1;
                break;

            default:
                printf("imginf: Incorrect usage (%c)\n", Optopt);
                imginf_help();
        }
    }


    for (i = Optind; i < argc; i++)
    {
        ret = file_seems_valid(argv[i]);
        if (!ret)
        {
            printf("Invalid file argument: %s\n", argv[i]);
            return 2;
        }
    }


    if (Optind == argc)
    {
        process_img_all();
        return 0;
    }

    /* Process each image on the command line. No wildcards are allowed */

    if (!g_verbose)
        status_header();

    for (i = Optind; i < argc; i++)
    {
        ret = file_seems_valid(argv[i]);

        if (ret == 1)
        {
            ret = process_image_png(argv[i], g_verbose);
            if (!g_verbose && ret)
                status_line(argv[i], ret);
        }
        else if (ret == 2)
        {
            ret = process_image_jpg(argv[i], g_verbose);
            if (!g_verbose && ret)
                status_line(argv[i], ret);
        }
    }

    return 0;
}


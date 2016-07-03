/*
 *  Dump out png information
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */


/* PNG specification:
    http://www.libpng.org/pub/png/spec/1.2/PNG-Contents.html
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "pnginf.h"


static const unsigned char png_sig[] = {
    0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'
};

static int g_verbose;

static struct {

    unsigned int width;
    unsigned int height;
    unsigned char depth;
    unsigned char colortype;

    unsigned int ppu_x, ppu_y;
    unsigned int dpi_x, dpi_y;
    unsigned int mm_x, mm_y;
    unsigned char ppu_spec;

} pnginf;




/*
 *  str_to_dword(buf) - Convert 4 bytes in buf to unsigned int
 */

static __inline__ unsigned int str_to_dword(const unsigned char *buf)
{
    return buf[3] | buf[2]<<8 | buf[1]<<16 | buf[0]<<24;
}




/*
 *  read_ihdr(fp) - Read a PNG IHDR
 *
 *  Returns 1 if ok, 0 if corrupt
 */

static int read_ihdr(FILE *fp, int clen)
{
    unsigned char buf[13];

    if (pnginf.width)
        return 0;

    if (clen != 13)
        return 0;

    if (fread(buf, 1, 13, fp) != 13)
        return 0;

    pnginf.width = str_to_dword(buf);
    pnginf.height = str_to_dword(buf + 4);
    pnginf.depth = buf[8];
    pnginf.colortype = buf[9];

    if (pnginf.width == 0 || pnginf.height == 0 || pnginf.depth == 0)
        return 0;

    if (!g_verbose)
        return 1;

    printf("    Width x Height: %u x %u\n", pnginf.width, pnginf.height);
    printf("    Bit depth: %u\n", pnginf.depth);

    printf("    Color type: ");

    switch (pnginf.colortype)
    {
        case 0: printf("grayscale\n");          break;
        case 2: printf("RGB\n");                break;
        case 3: printf("palette\n");            break;
        case 4: printf("grayscale + alpha\n");  break;
        case 6: printf("RGB + alpha\n");        break;
        default:
                printf("Unknown!\n");
    }

    return 1;
}




/*
 *  read_phys(fp) - Read a PNG pHYs
 *
 *  Returns 1 if ok, 0 if corrupt
 */

static int read_phys(FILE *fp, int clen)
{
    unsigned char buf[9];

    if (clen != 9)
        return 0;

    if (fread(buf, 1, 9, fp) != 9)
        return 0;

    pnginf.ppu_x = str_to_dword(buf);
    pnginf.ppu_y = str_to_dword(buf + 4);
    pnginf.ppu_spec = buf[8];

    if (pnginf.ppu_spec)
    {
        /* 1 Inch = 0.0254 meters */

        pnginf.dpi_x = ((pnginf.ppu_x * 254) / 1000) + 5;
        pnginf.dpi_x /= 10;

        pnginf.dpi_y = ((pnginf.ppu_y * 254) / 1000) + 5;
        pnginf.dpi_y /= 10;

        pnginf.mm_x = ((pnginf.width * 10000) / pnginf.ppu_x) + 5;
        pnginf.mm_x /= 10;
        pnginf.mm_y = ((pnginf.height * 10000) / pnginf.ppu_y) + 5;
        pnginf.mm_y /= 10;
    }

    if (!g_verbose)
        return 1;

    if (!pnginf.ppu_spec)
    {
        printf("    Pixels per unit: %u x %u\n", pnginf.ppu_x, pnginf.ppu_y);
    }
    else
    {
        printf("    Pixels per metre: %u x %u\n", pnginf.ppu_x, pnginf.ppu_y);
        printf("    Pixels per inch: %u x %u\n", pnginf.dpi_x, pnginf.dpi_y);
        printf("    Printed size (mm): %u x %u\n", pnginf.mm_x, pnginf.mm_y);
    }

    return 1;
}




/*
 *  read_text(fp) - Read a PNG tEXt
 *
 *  Returns 1 if ok, 0 if corrupt
 */

static int read_text(FILE *fp, int clen)
{
    int ch, avail;

    printf("    \"");
    avail = 60;

    while (clen)
    {
        clen--;
        if ((ch = getc(fp)) < 0)
            return 0;

        if (!ch)
            break;

        if (avail)
        {
            if (isprint(ch))
                printf("%c", ch);
            else
                printf("?");
            if (!--avail)
                printf(" ... ");
        }
    }

    if ( (clen && avail >= clen) || (!clen && avail >= 10) )
        printf("\" = ");
    else
        printf("\" = \n        ");

    if (!clen)
    {
        printf("(no value)");
    }
    else
    {
        printf("\"");

        avail = 60;

        while (clen--)
        {
            if ((ch = getc(fp)) < 0)
                return 0;

            if (ch == '\n')
                printf("\n        ");
            else if (isprint(ch))
                printf("%c", ch);
            else
                printf("?");

            if (!--avail)
            {
                avail = 60;
                if (ch != '\n')
                    printf("\n        ");
            }
        }

        printf("\"\n");
    }

    return 1;
}




/*
 *  read_chunk(fp) - Read a PNG file's "chunk"
 *
 *  Format: 4 byte length, 4 byte code, data, CRC32
 *
 *  Returns 1 if ok, 0 if end of PNG, -1 if corrupt
 */

static int read_chunk(FILE *fp)
{
    unsigned char buf[9];
    unsigned int chunklen;
    char *ccode;
    int i;

    if (fread(buf, 1, 8, fp) != 8)
        return -1;

    chunklen = str_to_dword(buf);
    if (chunklen > INT_MAX)
        return -1;

    for (i = 4; i < 8; i++)
    {
        if (!isalpha(buf[i]))
            return -1;
    }

    ccode = (char *)(buf + 4);
    ccode[4] = '\0';


    /* First chunk must be ihdr */

    if (!pnginf.width && strcmp(ccode, "IHDR") != 0)
        return -1;

    i = 4;  /* CRC at end is 4 bytes */


    if (strcmp(ccode, "IEND") == 0)
    {
        return 0;
    }
    else if (strcmp(ccode, "IHDR") == 0)
    {
        if (!read_ihdr(fp, chunklen))
            return -1;
    }
    else if (strcmp(ccode, "pHYs") == 0)
    {
        if (!read_phys(fp, chunklen))
            return -1;
    }
    else if (strcmp(ccode, "tEXt") == 0 && g_verbose)
    {
        if (!read_text(fp, chunklen))
            return -1;
    }
    else
    {
        /* Unhandled, absorb the chunk data */
        i += chunklen;
    }

    /* Read the CRC if handled or skip forward... */

    if (i <= sizeof(buf))
    {
        if (fread(buf, 1, i, fp) != i)
            return -1;
    }
    else
    {
        if (fseek(fp, i, SEEK_CUR) != 0)
            return -1;
    }

    return 1;
}




/*
 *  process_image_png(fname, isverbose) - Process a PNG file
 *
 *  Byte order is MSB first.
 *
 *  Returns: 0 on success
 *           1 on open failure
 *           2 if not a PNG file
 *           3 if a PNG file but corrupt
 */

int process_image_png(const char *fname, int isverbose)
{
    unsigned char pngheader[8];
    FILE *fp;
    int ret, i;

    g_verbose = isverbose;

    if (g_verbose)
        printf("[ %s ]\n\n", fname);

    fp = fopen(fname, "rb");
    if (fp == NULL)
    {
        if (g_verbose)
            printf("\tError: Cannot open file\n");
        return 1;
    }

    if (fread(pngheader, 1, 8, fp) != 8)
    {
        if (g_verbose)
            printf("\tError: File is not a PNG file\n");

        fclose(fp);
        return 2;
    }

    for (i = 0; i < sizeof(pngheader); i++)
    {
        if (pngheader[i] != png_sig[i])
        {
            if (g_verbose)
                printf("\tError: File is not a PNG file\n");

            fclose(fp);
            return 2;
        }
    }

    memset(&pnginf, 0, sizeof(pnginf));

    do {
        ret = read_chunk(fp);

        if (ret < 0)
        {
            if (g_verbose)
                printf("\tError: File is corrupt in some way\n");

            fclose(fp);
            return 3;
        }

    } while (ret);

    fclose(fp);

    if (g_verbose)
    {
        printf("\n\n");
    }
    else
    {
        printf("P  %5u   %5u   %2u    ",
                pnginf.width, pnginf.height, pnginf.depth);

        switch (pnginf.colortype)
        {
            case 0: printf("gry "); break;
            case 2: printf("RGB "); break;
            case 3: printf("palt"); break;
            case 4: printf("gryA"); break;
            case 6: printf("RGBA"); break;
            default:printf("Unkn");
        }

        if (pnginf.ppu_spec)
        {
            printf("  %5u  %3u x %3u  ",
                   pnginf.dpi_x, pnginf.mm_x, pnginf.mm_y);
        }
        else
        {
            printf("                    ");
        }

        if (strlen(fname) >= 30)
            printf("%.26s...\n", fname);
        else
            printf("%s\n", fname);
    }

    return 0;
}


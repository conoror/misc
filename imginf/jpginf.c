/*
 *  Dump out jpg information
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "imginf.h"


static struct {

    unsigned int width;
    unsigned int height;
    unsigned char depth;

    unsigned int res_x, res_y, res_unit;    /* Exif */
    unsigned int den_x, den_y, den_unit;    /* JFIF */

} jpginf;


struct ifdtagname
{
    unsigned int tag;
    const char *name;
};


static struct ifdtagname ifdtiff[] = {
    {   0x011A,     "X Resolution"      },
    {   0x011B,     "Y Resolution"      },
    {   0x0132,     "Last changed Time" },
    {   0x8769,     "Exif IFD Offset"   },
    {   0x8825,     "GPS IFD Offset"    },

    {   0x010E,     "Image description" },
    {   0x010F,     "Hardware make"     },
    {   0x0110,     "Hardware model"    },
    {   0x0131,     "Software"          },
    {   0x013B,     "Artist"            },
    {   0x8298,     "Copyright"         },
};


struct ifdtagname ifdexif[] = {
    {   0x9003,     "Original Time"     },
    {   0x829A,     "Exposure Time(s)"  },
    {   0x829D,     "F-stop"            },
    {   0x9201,     "Shutter speed(s)"  },
    {   0x9202,     "Lens Aperture"     },
    {   0x9203,     "Brightness"        },
    {   0x9204,     "Exposure Bias"     },
    {   0x9205,     "Max Aperture"      },
    {   0x9206,     "Subject distance(m)" },
    {   0x920A,     "Focal length(mm)"  },
    {   0xA20B,     "Flash energy"      },
    {   0xA215,     "Exposure Index"    },
    {   0xA404,     "Digital zoom ratio"},
    {   0xA407,     "Gain control"      },
    {   0xA420,     "Unique ID"         },
};


struct ifdtagname ifdgps[] = {
    {   0x02,   "Latitude"  },
    {   0x04,   "Longitude" },
    {   0x06,   "Altitude"  },
    {   0x07,   "Time"      },
    {   0x0D,   "Speed"     },
    {   0x0F,   "Moving"    },
    {   0x11,   "Pointing"  },
    {   0x1D,   "Date"      },
};




/*
 *  ifdcompare(a, b) - For qsort to compare on ifd offsets
 */

int ifdcompare(const void *a, const void *b)
{
    /* Sort on the offsets */

    struct ifdentry *ifda = (struct ifdentry *)a;
    struct ifdentry *ifdb = (struct ifdentry *)b;

    return ifda->off - ifdb->off;
}




/*
 *  getifdname(tag, tntype) - Return attribute name given tag
 *  Use one of the tiff or exif or gps lookup tables
 */

const char *getifdname(unsigned int tag, int tntype)
{
    static const char *dummy = "Unknown attribute tag";
    struct ifdtagname *tnarr;
    int tnlen;
    int i;

    if (tntype == IFD_TN_TIFF)
    {
        tnarr = ifdtiff;
        tnlen = NELEMS(ifdtiff);
    }
    else if (tntype == IFD_TN_EXIF)
    {
        tnarr = ifdexif;
        tnlen = NELEMS(ifdexif);
    }
    else if (tntype == IFD_TN_GPS)
    {
        tnarr = ifdgps;
        tnlen = NELEMS(ifdgps);
    }
    else
    {
        return dummy;
    }

    for (i = 0; i < tnlen; i++)
    {
        if (tag == tnarr[i].tag)
            return tnarr[i].name;
    }

    return dummy;
}




/*
 *  fp_move_forward(fp, hop) - Uses a buffer to move fp forward by hop
 *
 *  Uses a small buffer to move forward by small amounts otherwise
 *  use fseek. Yes, this does assume fseek is completely stupid, but
 *  I've seen some pretty dim implementations that throw everything out,
 *  move the file pointer and then reread the buffer...
 *
 *  Returns: 1 on success, 0 on error
 */

int fp_move_forward(FILE *fp, int hop)
{
    char buf[256];

    if (!fp || hop < 0)
        return 0;

    if (!hop)
        return 1;

    if (hop <= sizeof(buf))
    {
        if (fread(buf, 1, hop, fp) != hop)
            return 0;
    }
    else
    {
        if (fseek(fp, hop, SEEK_CUR) != 0)
            return 0;
    }

    return 1;
}




/*
 *  read_sofx(fp, clen) - Read a SOF0(1,2,3) jpeg segment
 *
 *  Start of Frame segment contains height, width, depth
 *  Function absorbs all bytes passed in clen
 *  (reads entire segment)
 *
 *  Returns: 1 if ok, 0 if corrupt
 */

static int read_sofx(FILE *fp, int clen)
{
    unsigned char buf[6];

    if (clen < 6)
        return 0;

    if (fread(buf, 1, 6, fp) != 6)
        return 0;

    clen -= 6;

    /* Precision, 2 byte height, 2 byte width, components */

    jpginf.depth = buf[0];
    jpginf.height = strbe_to_word(buf + 1);
    jpginf.width = strbe_to_word(buf + 3);

    if (jpginf.width == 0 || jpginf.height == 0 || jpginf.depth == 0)
        return 0;

    if (g_verbose)
    {
        printf("\n"
               "    Frame\n"
               "    -----\n");
        printf("    Width: %u\n", jpginf.width);
        printf("    Height: %u\n", jpginf.height);
        printf("    Bit depth: %u\n", jpginf.depth);

        switch (buf[5])
        {
            case 1: printf("    Components: Greyscale\n"); break;
            case 3: printf("    Components: YCbCr\n"); break;
            case 4: printf("    Components: CMYK\n"); break;
        }
    }

    return fp_move_forward(fp, clen);
}




/*
 *  read_tiff(fp, clen) - Read an entire TIFF block
 *
 *  Within APP1, lies a TIFF section and within that section
 *  lies a number of IFDs (Exif, GPS). The TIFF header point to
 *  the TIFF IFD and specifies endianness. The Tiff IFD points
 *  to the exif and gps IFDs, both of which are optional. The
 *  processing for any IFD is pretty similar so this function
 *  looks like read_ifd_exif and read_ifd_gps for the most part.
 *
 *  This function reads the TIFF section and dumps data if
 *  g_verbose is set. Otherwise just grabs res_[x,y,unit]
 *
 *  Returns: number of bytes absorbed from input if ok
 *           0 if corrupt
 */

static int read_tiff(FILE *fp, int clen)
{
    unsigned char buf[20];
    struct ifdentry ifdent[20];
    int tiffoff, nents, i, isle, nfields, bhop;

    nents = tiffoff = 0;


    /* At the TIFF header. 2 byte endian type then fixed word
       of 0x002A (to check endian) then 4 byte IFD0 offset */

    if (clen < 8)
        return 0;

    if (fread(buf, 1, 8, fp) != 8)
        return 0;

    clen -= 8; tiffoff += 8;

    if (buf[0] == 0x49 && buf[1] == 0x49)       /* Little endian */
        isle = 1;
    else if (buf[0] == 0x4D && buf[1] == 0x4D)  /* Big endian */
        isle = 0;
    else                                        /* Messed up */
        return 0;

    if (str_to_word(buf + 2, isle) != 0x2A)     /* Check word */
        return 0;

    bhop = str_to_dword(buf + 4, isle);     /* IFD offset */
    bhop -= 8;                              /* From zero  */

    if (bhop > clen)
        return 0;

    if (!fp_move_forward(fp, bhop))
        return 0;

    clen -= bhop; tiffoff += bhop;


    /* At the TIFF IFD. 2 byte count, then count number
       of fields then a 4 byte pointer to next IFD */

    if (clen < 6)
        return 0;

    if (fread(buf, 1, 2, fp) != 2)
        return 0;

    clen -=2; tiffoff += 2;

    nfields = str_to_word(buf, isle);

    if (clen < nfields * 12 + 4)
        return 0;

    if (g_verbose)
    {
        printf("\n"
               "    Image (TIFF)\n"
               "    ------------\n");
    }


    /* Read fields. Just like before */

    for (i = 0; i < nfields; i++)
    {
        unsigned int tag, type, cnt, dval;

        if (fread(buf, 1, 12, fp) != 12)
            return 0;

        clen -= 12; tiffoff += 12;

        tag = str_to_word(buf + 0, isle);
        type = str_to_word(buf + 2, isle);

        cnt = str_to_dword(buf + 4, isle);
        dval = str_to_dword(buf + 8, isle);

        switch (tag)
        {
            case 0x010E:        /* ImageDescription */
            case 0x010F:        /* Make */
            case 0x0110:        /* Model */
            case 0x0131:        /* Software */
            case 0x013B:        /* Artist */
            case 0x8298:        /* Copyright */

                /* These are all ascii so in theory could be 3 characters
                   and a null terminator. Thus fits in the val field and
                   can be output right away... */

                if (g_verbose && cnt <= 4)
                {
                    char *str = (char *)buf + 8;

                    buf[11] = 0;    /* Terminate, just in case */

                    printf("    %s: ", getifdname(tag, IFD_TN_TIFF));

                    if (!*str)
                        printf("(no value)");

                    while (*str)
                    {
                        if (isprint(*str))
                            printf("%c", *str);
                        else
                            printf("?");

                        str++;
                    }
                    printf("\n");
                }

                if (cnt <= 4)
                    break;

                /* if cnt > 4 we fall through to next group */

            case 0x011A:        /* Xresolution */
            case 0x011B:        /* Y resolution */
            case 0x0132:        /* DateTime */
            case 0x8769:        /* Exif IFD Offset */
            case 0x8825:        /* GPS IFD Offset */

                if (nents >= NELEMS(ifdent))
                {
                    printf("Internal error, out of space in tiff parser\n");
                    return 0;
                }

                ifdent[nents].name = getifdname(tag, IFD_TN_TIFF);
                ifdent[nents].tag  = tag;
                ifdent[nents].type = type;
                ifdent[nents].cnt  = cnt;
                ifdent[nents].off  = dval;
                nents++;

            break;


            case 0x0128:        /* ResolutionUnit */

                /* Subtract one to be same as JFIF. Honestly... */

                jpginf.res_unit = str_to_word(buf + 8, isle) - 1;

                if (g_verbose)
                {
                    printf("    Resolution Unit: ");
                    if (jpginf.res_unit == 1)
                        printf("pixels per inch\n");
                    else if (jpginf.res_unit == 2)
                        printf("pixels per cm\n");
                }

            break;


        }   /* end switch */

    } /* End for */


    /* sort tags by the offset they appear at */

    qsort(ifdent, nents, sizeof(ifdent[0]), ifdcompare);


    /* Scan the rest of the Tiff IFD block */

    for (i = 0; i < nents; i++)
    {
        /* Move to the right offset. Currently at: tiffoff */

        bhop = ifdent[i].off - tiffoff;
        if (bhop > clen)
            return 0;

        if (!fp_move_forward(fp, bhop))
            return 0;

        clen -= bhop; tiffoff += bhop;

        if (tiffoff != ifdent[i].off)
            return 0;


        if (g_verbose && ifdent[i].tag == 0x8769)
        {
            /* An entire EXIF IFD block, somewhere up further */

            int ret = read_ifd_exif(fp, clen, tiffoff, isle);
            if (!ret || ret > clen)
                return 0;

            clen -= ret;
            tiffoff += ret;
        }

        else if (g_verbose && ifdent[i].tag == 0x8825)
        {
            /* GPS IFD offset. Not processed at the moment */

            int ret = read_ifd_gps(fp, clen, tiffoff, isle);
            if (!ret || ret > clen)
                return 0;

            clen -= ret;
            tiffoff += ret;
        }

        else if (ifdent[i].tag == 0x011A || ifdent[i].tag == 0x011B)
        {
            /* Image resolution, x and y */
            /* We have two longs in the form of a fraction */

            uint32_t num, den;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; tiffoff += 8;

            num = str_to_dword(buf + 0, isle);
            den = str_to_dword(buf + 4, isle);

            if (den == 0)
                den = 1;

            frac = (double)num/(double)den;

            if (g_verbose)
                printf("    %s: %.0f\n", ifdent[i].name, frac);

            if (ifdent[i].tag == 0x011A)
                jpginf.res_x = (int)(frac + 0.5);      /* Round properly */
            else
                jpginf.res_y = (int)(frac + 0.5);
        }

        else if (ifdent[i].type == 2 && g_verbose)
        {
            /* ASCII (type 2): Same as with exif:
               string of length ifdent[i].cnt including terminator.
               This string can be terminated early (generally to stop
               having to pack it into bytes inside the value)
            */

            int ch, k;

            if (clen < ifdent[i].cnt)
                return 0;

            printf("    %s: ", ifdent[i].name);

            for (k = 0; k < ifdent[i].cnt; k++)
            {
                ch = getc(fp);
                if (ch < 0)
                    return 0;

                clen--; tiffoff++;

                if (ch == 0)
                    break;
                else if (isprint(ch))
                    printf("%c", ch);
                else
                    printf("?");
            }

            if (k == 0)
                printf("(no value)");
            printf("\n");

            if (k >= ifdent[i].cnt)
                return 0;       /* Unterminated */
        }

    }   /* end for() */

    return tiffoff;
}




/*
 *  read_app1(fp, clen) - Read the jpg app1 segment (TIFF/Exif)
 *
 *  This reads and dumps all information in block. Hope to
 *  gather res_x, res_y and res_unit at the very least
 *
 *  Function absorbs all bytes passed in clen
 *  (reads entire segment)
 *
 *  Returns: 1 if ok, 0 if corrupt
 */

static int read_app1(FILE *fp, int clen)
{
    char buf[6];
    int ret;

    if (clen < 14)
        return 0;

    /* APP1 "EXIF\0\0" ident is immediately followed
       by a TIFF Header and one or two IFDs */

    if (fread(buf, 1, 6, fp) != 6)
        return 0;

    clen -= 6;

    if (buf[4] != 0 || buf[5] != 0)
        return 0;

    if (strcmp(buf, "Exif") != 0)
        return 0;

    /* Now at the very beginning of the TIFF header */

    ret = read_tiff(fp, clen);
    if (!ret || ret > clen)
        return 0;

    clen -= ret;
    return fp_move_forward(fp, clen);
}




/*
 *  read_app0(fp, clen) - Read the jpg app0 segment, JFIF
 *
 *  There can be two with the same id (why!!!)
 *
 *  This reads and dumps all information in block. Should
 *  gather den_x, den_y, den_unit
 *
 *  Function absorbs all bytes passed in clen
 *  (reads entire segment)
 *
 *  Returns: 1 if ok, 0 if corrupt
 */

static int read_app0(FILE *fp, int clen)
{
    unsigned char buf[10];

    if (clen < 5)
        return 0;

    if (fread(buf, 1, 5, fp) != 5)
        return 0;

    clen -= 5;

    if (buf[4] != 0)
        return 0;

    if (strcmp((char *)buf, "JFIF") != 0)
    {
        /* Not the end of the world as we can have
           a JFXX header instead which is skipped */

        return fp_move_forward(fp, clen);
    }

    if (clen < 7)
        return 0;

    if (fread(buf, 1, 7, fp) != 7)
        return 0;

    clen -= 7;

    jpginf.den_unit = buf[2];
    jpginf.den_x = strbe_to_word(buf + 3);
    jpginf.den_y = strbe_to_word(buf + 5);

    if (g_verbose)
    {
        printf("\n"
               "    Image (JFIF)\n"
               "    ------------\n");

        if (jpginf.den_unit == 0)
        {
            printf("    XY Aspect Ratio: %u:%u\n",
                    jpginf.den_x, jpginf.den_y);
        }
        else
        {
            printf("    X Density: %u\n", jpginf.den_x);
            printf("    Y Density: %u\n", jpginf.den_y);

            printf("    Density Unit: ");

            if (jpginf.den_unit == 1)
                printf("pixels per inch\n");
            else if (jpginf.den_unit == 2)
                printf("pixels per cm\n");
            else
                printf("Unknown\n");
        }
    }

    return fp_move_forward(fp, clen);
}




/*
 *  read_marker(fp) - Read a JPG file's marker segment
 *
 *  Format: 2 byte id + optional 2 byte length ... data
 *
 *  Returns: 1 if ok, 0 if end of JPG, -1 if corrupt
 */

static int read_marker(FILE *fp)
{
    unsigned char buf[4];
    int clen, ret, id;

    if (fread(buf, 1, 2, fp) != 2)
        return -1;

    if (buf[0] != 0xFF)
        return -1;

    while (buf[1] == 0xFF)
    {
        /* 0xFF is fill padding, keep chucking it away... */

        if ((ret = getc(fp)) < 0)
            return -1;
        buf[1] = ret;
    }

    id = buf[1];


    if (id == 0xD9 || id == 0xDA)
    {
        /* end of image or start of scan. We're done */
        return 0;
    }

    /* Read the 2 byte content length, big endian */

    if (fread(buf, 1, 2, fp) != 2)
        return -1;

    clen = strbe_to_word(buf) - 2;
    if (clen < 0)
        return -1;


    ret = 1;    /* Start with assuming all is just dandy */

    if (id >= 0xC0 && id <= 0xC3)   /* SOF */
    {
        ret = read_sofx(fp, clen);
    }
    else if (id == 0xe0)            /* APP0 */
    {
        ret = read_app0(fp, clen);
    }
    else if (id == 0xe1)            /* APP1 */
    {
        ret = read_app1(fp, clen);
    }
    else
    {
        if (!fp_move_forward(fp, clen))
            return -1;
    }

    if (!ret)
        return -1;

    return 1;
}




/*
 *  process_image_jpg(fname, isverbose) - Process a JPG file
 *
 *  Returns: 0 on success
 *           1 on open failure
 *           2 if not a JPG file
 *           3 if a JPG file but corrupt
 */

int process_image_jpg(const char *fname)
{
    unsigned char jpgsoi[2];
    FILE *fp;
    int ret;

    if (g_verbose)
        printf("[ %s ]\n", fname);

    fp = fopen(fname, "rb");
    if (fp == NULL)
    {
        if (g_verbose)
            printf("\tError: Cannot open file\n");
        return 1;
    }

    if (fread(jpgsoi, 1, 2, fp) != 2)
    {
        if (g_verbose)
            printf("\tError: File is not a JPG file\n");

        fclose(fp);
        return 2;
    }

    if (jpgsoi[0] != 0xff || jpgsoi[1] != 0xd8)
    {
        if (g_verbose)
            printf("\tError: File is not a JPEG file\n");

        fclose(fp);
        return 2;
    }

    memset(&jpginf, 0, sizeof(jpginf));

    do {
        if ((ret = read_marker(fp)) < 0)
        {
            if (g_verbose)
                printf("\tError: File is corrupt in some way\n");

            fclose(fp);
            return 3;
        }

    } while (ret);

    fclose(fp);


    if (jpginf.width == 0 || jpginf.height == 0)
    {
        if (g_verbose)
            printf("No Frame information. File is not valid\n");
        return 3;
    }


    if (g_verbose)
    {
        printf("\n\n");
    }
    else
    {
        printf("J  %5u   %5u   %2u    ",
                jpginf.width, jpginf.height, jpginf.depth);

        printf("    ");         /* No colortype. Just no! */

        /* Prioritise JFIF information. I have to pick one! */

        if (jpginf.den_x)
        {
            jpginf.res_x = jpginf.den_x;
            jpginf.res_y = jpginf.den_y;
            jpginf.res_unit = jpginf.den_unit;
        }

        if (jpginf.res_x)
        {
            if (jpginf.res_unit == 1)       /* Inch */
            {
                printf("  %5u  %3.0f x %3.0f  ",
                    jpginf.res_x,
                    ((double)jpginf.width / (double)jpginf.res_x) * 25.4,
                    ((double)jpginf.height / (double)jpginf.res_x) * 25.4);
            }

            else if (jpginf.res_unit == 2)  /* cm */
            {
                printf("  %5.0f  %3.0f x %3.0f  ",
                    (double)jpginf.res_x * 2.54,
                    ((double)jpginf.width / (double)jpginf.res_x) * 10,
                    ((double)jpginf.height / (double)jpginf.res_x) * 10);
            }

            else
            {
                printf("                    ");
            }
        }

        if (strlen(fname) >= 30)
            printf("%.26s...\n", fname);
        else
            printf("%s\n", fname);
    }

    return 0;
}


/*
 *  Dump out jpg information
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */


/* Exif spec: http://www.exif.org/Exif2-2.PDF */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <errno.h>

#include "jpginf.h"


#ifndef NELEMS
#define NELEMS(a)  (sizeof(a) / sizeof((a)[0]))
#endif


static int g_verbose;
static unsigned char g_buf[256];


static struct {

    unsigned int width;
    unsigned int height;
    unsigned char depth;

    unsigned int res_x, res_y, res_unit;    /* Exif */
    unsigned int den_x, den_y, den_unit;    /* JFIF */

} jpginf;


static struct ifdattr
{
    const char  *name;
    unsigned int tag;
    unsigned int type;
    unsigned int cnt;
    unsigned int off;

} g_ifdatt[20];


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


static struct ifdtagname ifdexif[] = {
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




/*
 *  ifdcompare(a, b) - For qsort to compare on ifd offsets
 */

static int ifdcompare(const void *a, const void *b)
{
    /* Sort on the offsets */

    struct ifdattr *ifda = (struct ifdattr *)a;
    struct ifdattr *ifdb = (struct ifdattr *)b;

    return ifda->off - ifdb->off;
}




/*
 *  getifdname(tag, istiff) - Return attribute name given tag
 *  Use tiff or exif lookup table
 */

static const char *getifdname(unsigned int tag, int istiff)
{
    static const char *dummy = "Unknown attribute tag";
    struct ifdtagname *tnp;
    int tnlen;
    int i;

    if (istiff)
    {
        tnp = ifdtiff;
        tnlen = NELEMS(ifdtiff);
    }
    else
    {
        tnp = ifdexif;
        tnlen = NELEMS(ifdexif);
    }

    for (i = 0; i < tnlen; i++)
    {
        if (tag == tnp[i].tag)
            return tnp[i].name;
    }

    return dummy;
}




/*
 *  Some handy inlines for string to word/dword conversion
 */

static __inline__ unsigned int strbe_to_word(const unsigned char *str)
{
    return str[1] | str[0]<<8;
}

static __inline__ unsigned int strle_to_word(const unsigned char *str)
{
    return str[0] | str[1]<<8;
}

static __inline__ unsigned int str_to_word(const unsigned char *str, int isle)
{
    if (isle)
        return str[0] | str[1]<<8;
    return str[1] | str[0]<<8;
}

static __inline__ unsigned int str_to_dword(const unsigned char *str, int isle)
{
    if (isle)
        return str[0] | str[1]<<8 | str[2]<<16 | str[3]<<24;
    return str[3] | str[2]<<8 | str[1]<<16 | str[0]<<24;
}




/*
 *  fp_move_forward(fp, hop) - Use g_buf to move fp forward by hop
 *
 *  Use the global buffer to move forward by small amounts otherwise
 *  use fseek. This does assume fseek is stupider than it should be,
 *  but I've seen plenty of pretty dim implementations that throw
 *  everything out, move the file pointer and then reread the buffer
 *
 *  Returns: 1 on success, 0 on error
 */

static int fp_move_forward(FILE *fp, int hop)
{
    if (!fp || hop < 0)
        return 0;

    if (!hop)
        return 1;

    if (hop <= sizeof(g_buf))
    {
        if (fread(g_buf, 1, hop, fp) != hop)
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
 *  Uses g_buf.
 *
 *  Returns: 1 if ok, 0 if corrupt
 */

static int read_sofx(FILE *fp, int clen)
{
    if (clen < 6)
        return 0;

    if (fread(g_buf, 1, 6, fp) != 6)
        return 0;

    clen -= 6;

    /* Precision, 2 byte height, 2 byte width, components */

    jpginf.depth = g_buf[0];
    jpginf.height = strbe_to_word(g_buf + 1);
    jpginf.width = strbe_to_word(g_buf + 3);

    if (jpginf.width == 0 || jpginf.height == 0 || jpginf.depth == 0)
        return 0;

    if (g_verbose)
    {
        printf("\n    Frame\n");
        printf("    -----\n");
        printf("    Width: %u\n", jpginf.width);
        printf("    Height: %u\n", jpginf.height);
        printf("    Bit depth: %u\n", jpginf.depth);

        switch (g_buf[5])
        {
            case 1: printf("    Components: Greyscale\n"); break;
            case 3: printf("    Components: YCbCr\n"); break;
            case 4: printf("    Components: CMYK\n"); break;
        }
    }

    return fp_move_forward(fp, clen);
}




/*
 *  read_ifd_exif(fp, clen) - Read an EXIF Tiff IFD section
 *
 *  Within APP1, lies a TIFF section and within that secton
 *  lies a number of IFDs, one of which is an Exif IFD.
 *  This function reads that section and dumps data (does
 *  not use g_verbose - only colorspace is available)
 *
 *  Uses g_buf. Caller needs to know how much was read.
 *  Function needs to know where it is in tiff block.
 *
 *  Returns: number of bytes absorbed from input if ok
 *           0 if corrupt
 */

static int read_ifd_exif(FILE *fp, int clen, int tiffoff, int isle)
{
    int exifoff, natts, nfields, i;

    natts = 0;              /* Offset in g_ifdatt[] */
    exifoff = tiffoff;      /* Offset rel to start of tiff */


    /* At the EXIF IFD. 2 byte count, then fields
       then 4 byte pointer to next IFD */

    if (clen < 6)
        return 0;

    if (fread(g_buf, 1, 2, fp) != 2)
        return 0;

    clen -=2; exifoff += 2;

    nfields = str_to_word(g_buf, isle);

    if (clen < nfields * 12 + 4)
        return 0;


    printf("\n    Camera\n");
    printf("    ------\n");


    /* Read fields */

    for (i = 0; i < nfields; i++)
    {
        unsigned int tag, type, cnt, val, w;

        if (fread(g_buf, 1, 12, fp) != 12)
            return 0;

        clen -= 12; exifoff += 12;

        tag = str_to_word(g_buf + 0, isle);
        type = str_to_word(g_buf + 2, isle);

        cnt = str_to_dword(g_buf + 4, isle);
        val = str_to_dword(g_buf + 8, isle);

        switch (tag)
        {
            case 0x9003:        /* DateTimeOriginal */
            case 0x829A:        /* ExposureTime */
            case 0x829D:        /* FNumber */

            case 0x9201:        /* ShutterSpeed */
            case 0x9202:        /* Aperture */
            case 0x9203:        /* Brightness */
            case 0x9204:        /* ExposureBias */
            case 0x9205:        /* Max aperture */
            case 0x9206:        /* SubjectDistance */

            case 0x920A:        /* FocalLength */
            case 0xA20B:        /* FlashEnergy */
            case 0xA215:        /* ExposureIndex */
            case 0xA404:        /* DigitalZoomRatio */
            case 0xA407:        /* GainControl */
            case 0xA420:        /* ImageUniqueID */

                if (natts >= NELEMS(g_ifdatt))
                    return 0;

                g_ifdatt[natts].name = getifdname(tag, 0);
                g_ifdatt[natts].tag  = tag;
                g_ifdatt[natts].type = type;
                g_ifdatt[natts].cnt  = cnt;
                g_ifdatt[natts].off  = val;
                natts++;

            break;


            case 0xA001:        /* ColorSpace */

                w = str_to_word(g_buf + 8, isle);
                printf("    Color space: ");
                if (w == 1)
                    printf("sRGB\n");
                else if (w == 0xFFFF)
                    printf("Uncalibrated\n");
                else
                    printf("Reserved\n");

            break;


            case 0x8822:        /* ExposureProgram */

                w = str_to_word(g_buf + 8, isle);
                printf("    Exposure program: ");
                switch (w)
                {
                    case 1: printf("Manual\n");
                            break;
                    case 2: printf("Normal\n");
                            break;
                    case 3: printf("Aperture priority\n");
                            break;
                    case 4: printf("Shutter priority\n");
                            break;
                    case 5: printf("Creative (depth of field bias)\n");
                            break;
                    case 6: printf("Action (shutter speed bias)\n");
                            break;
                    case 7: printf("Portrait mode\n");
                            break;
                    case 8: printf("Landscape mode\n");
                            break;
                    default:
                            printf("Not defined\n");
                }

            break;


            case 0x8827:        /* ISOSpeedRatings */

                w = str_to_word(g_buf + 8, isle);
                printf("    Speed rating: ISO-%u\n", w);

            break;


            case 0x9207:        /* MeteringMode */

                w = str_to_word(g_buf + 8, isle);
                printf("    Metering mode: ");
                switch (w)
                {
                    case 0: printf("Unknown\n");
                            break;
                    case 1: printf("Average\n");
                            break;
                    case 2: printf("Center weighted average\n");
                            break;
                    case 3: printf("Spot\n");
                            break;
                    case 4: printf("MultiSpot\n");
                            break;
                    case 5: printf("Pattern\n");
                            break;
                    case 6: printf("Partial\n");
                            break;
                    default:
                            printf("Other\n");
                }

            break;


            case 0x9208:        /* LightSource */

                w = str_to_word(g_buf + 8, isle);
                printf("    Light source: ");
                switch (w)
                {
                    case  0: printf("Unknown\n");
                             break;
                    case  1: printf("Daylight\n");
                             break;
                    case  2: printf("Fluorescent\n");
                             break;
                    case  3: printf("Incandescent\n");
                             break;
                    case  4: printf("Flash\n");
                             break;
                    case  9: printf("Fine weather\n");
                             break;
                    case 10: printf("Cloudy weather\n");
                             break;
                    case 11: printf("Shade\n");
                             break;
                    case 12: printf("Daylight fluorescent\n");
                             break;
                    case 13: printf("Day white fluorescent\n");
                             break;
                    case 14: printf("Cool white fluorescent\n");
                             break;
                    case 15: printf("White fluorescent\n");
                             break;
                    case 17: printf("Standard light A\n");
                             break;
                    case 18: printf("Standard light B\n");
                             break;
                    case 19: printf("Standard light C\n");
                             break;
                    case 20: printf("D55\n");
                             break;
                    case 21: printf("D65\n");
                             break;
                    case 22: printf("D75\n");
                             break;
                    case 23: printf("D50\n");
                             break;
                    case 24: printf("ISO studio tungsten\n");
                             break;
                    default:
                                printf("Other\n");
                }

            break;


            case 0x9209:        /* Flash */

                w = str_to_word(g_buf + 8, isle);
                printf("    Flash: ");
                switch (w)
                {
                    case 0x00: printf("No flash\n");
                               break;
                    case 0x01: printf("Flash\n");
                               break;
                    case 0x05: printf("Flash,No detect\n");
                               break;
                    case 0x07: printf("Flash,Detected\n");
                               break;
                    case 0x09: printf("Flash,Compulsory\n");
                               break;
                    case 0x0D: printf("Flash,Compulsory,No detect\n");
                               break;
                    case 0x0F: printf("Flash,Compulsory,Detected\n");
                               break;
                    case 0x10: printf("No flash,Compulsory\n");
                               break;
                    case 0x18: printf("No flash,Auto\n");
                               break;
                    case 0x19: printf("Flash,Auto\n");
                               break;
                    case 0x1D: printf("Flash,Auto,No detect\n");
                               break;
                    case 0x1F: printf("Flash,Auto,Detected\n");
                               break;
                    case 0x20: printf("No flash function\n");
                               break;
                    case 0x41: printf("Flash,Red-eye\n");
                               break;
                    case 0x45: printf("Flash,Red-eye,No detect\n");
                               break;
                    case 0x47: printf("Flash,Red-eye,Detected\n");
                               break;
                    case 0x49: printf("Flash,Compulsory,Red-eye\n");
                               break;
                    case 0x4D: printf("Flash,Compulsory,Red-eye,No detect\n");
                               break;
                    case 0x4F: printf("Flash,Compulsory,Red-eye,Detected\n");
                               break;
                    case 0x59: printf("Flash,Auto,Red-eye\n");
                               break;
                    case 0x5D: printf("Flash,Auto,Red-eye,No detect\n");
                               break;
                    case 0x5F: printf("Flash,Auto,Red-eye,Detected\n");
                               break;
                    default:
                               printf("Reserved\n");
                }

            break;


            case 0xA402:        /* ExposureMode */

                w = str_to_word(g_buf + 8, isle);
                printf("    Exposure mode: ");
                switch (w)
                {
                    case 0: printf("Auto exposure\n");
                            break;
                    case 1: printf("Manual exposure\n");
                            break;
                    case 2: printf("Auto bracket\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


            case 0xA403:        /* WhiteBalance */

                w = str_to_word(g_buf + 8, isle);
                printf("    White balance: ");
                switch (w)
                {
                    case 0: printf("Auto white balance\n");
                            break;
                    case 1: printf("Manual white balance\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


            case 0xA406:        /* SceneCaptureType */
                w = str_to_word(g_buf + 8, isle);
                printf("    Scene capture type: ");
                switch (w)
                {
                    case 0: printf("Standard\n");
                            break;
                    case 1: printf("Landscape\n");
                            break;
                    case 2: printf("Portrait\n");
                            break;
                    case 3: printf("Night scene\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


            case 0xA408:        /* Contrast */

                w = str_to_word(g_buf + 8, isle);
                printf("    Contrast: ");
                switch (w)
                {
                    case 0: printf("Normal\n");
                            break;
                    case 1: printf("Soft\n");
                            break;
                    case 2: printf("Hard\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


            case 0xA409:        /* Saturation */

                w = str_to_word(g_buf + 8, isle);
                printf("    Saturation: ");
                switch (w)
                {
                    case 0: printf("Normal\n");
                            break;
                    case 1: printf("Low\n");
                            break;
                    case 2: printf("High\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


            case 0xA40A:        /* Sharpness */

                w = str_to_word(g_buf + 8, isle);
                printf("    Sharpness: ");
                switch (w)
                {
                    case 0: printf("Normal\n");
                            break;
                    case 1: printf("Soft\n");
                            break;
                    case 2: printf("Hard\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


            case 0xA40C:        /* SubjectDistanceRange */

                w = str_to_word(g_buf + 8, isle);
                printf("    Subject distance range: ");
                switch (w)
                {
                    case 0: printf("Unknown\n");
                            break;
                    case 1: printf("Macro\n");
                            break;
                    case 2: printf("Close\n");
                            break;
                    case 3: printf("Distant\n");
                            break;
                    default:
                            printf("Reserved\n");
                }

            break;


        }   /* end switch */


    } /* End for */


    /*
     *  sort tags by the offset they appear at
     *  this doesn't appear to be necessary most of the time but
     *  it would make a mess if not done.
     */

    qsort(g_ifdatt, natts, sizeof(g_ifdatt[0]), ifdcompare);


    /* Scan the rest of the IFD block and dump the rest of the info */

    for (i = 0; i < natts; i++)
    {
        int bhop;

        /* Move to the given offset. Currently at: exifoff */

        bhop = g_ifdatt[i].off - exifoff;
        if (bhop > clen)
            return 0;

        if (!fp_move_forward(fp, bhop))
            return 0;

        clen -= bhop; exifoff += bhop;

        if (exifoff != g_ifdatt[i].off)        /* Just in case! */
            return 0;


        if (g_ifdatt[i].type == 2)
        {
            /* ASCII: 0x9003, 0xA420 tags

               string of length g_ifdatt[i].cnt including terminator.
               This string can be terminated early (generally to stop
               having to pack it into bytes inside the value)
            */

            int ch, k;

            if (clen < g_ifdatt[i].cnt)
                return 0;

            printf("    %s: ", g_ifdatt[i].name);

            for (k = 0; k < g_ifdatt[i].cnt; k++)
            {
                ch = getc(fp);
                if (ch < 0)
                    return 0;

                clen--; exifoff++;

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

            if (k >= g_ifdatt[i].cnt)
                return 0;       /* Unterminated */
        }


        else if (g_ifdatt[i].type == 5)
        {
            /* Rational: Two ulongs in a fraction. I only
               use those for which .cnt is 1. Try not to do
               stupid things like divide by zero! */

            uint32_t num, den, tag;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(g_buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; exifoff += 8;

            num = str_to_dword(g_buf + 0, isle);
            den = str_to_dword(g_buf + 4, isle);
            tag = g_ifdatt[i].tag;

            printf("    %s: ", g_ifdatt[i].name);

            if (den == 0)
                den = 1;        /* Yeah I've seen it happen already... */

            frac = (double)num/(double)den;

            if (tag == 0x829A)          /* ExposureTime */
            {
                if (frac >= 1.0 || frac == 0.0)
                    printf("%.1f\n", frac);
                else
                    printf("1/%.0f\n", 1.0/frac);
            }

            else if (tag == 0x829D)     /* F-stop */
            {
                printf("f/%.1f\n", frac);
            }

            else if (tag == 0x9202 ||   /* Aperture */
                     tag == 0x9205  )   /* Max aperture */
            {
                errno = 0;
                frac = pow(2, frac/2.0);
                if (errno)
                    printf("Unknown\n");
                else
                    printf("f/%.1f\n", frac);
            }

            else if (tag == 0x9206)     /* Distance */
            {
                if (num == 0xFFFFFFFF)
                    printf("Infinity\n");

                else if (num == 0 || den == 0)
                    printf("Unknown\n");

                else
                    printf("%.2f\n", frac);
            }

            else
            {
                printf("%.1f\n", frac);
            }
        }


        else if (g_ifdatt[i].type == 10)
        {
            /* Srational: Two slongs in a fraction.
               Only: 0x9201, 0x9203, 0x9204 */

            int32_t snum, sden;
            unsigned int tag;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(g_buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; exifoff += 8;

            snum = (int32_t)str_to_dword(g_buf + 0, isle);
            sden = (int32_t)str_to_dword(g_buf + 4, isle);
            tag = g_ifdatt[i].tag;

            printf("    %s: ", g_ifdatt[i].name);

            if (sden == 0)
                sden = 1;

            frac = (double)snum/(double)sden;

            if (tag == 0x9201)          /* Shutter Speed */
            {
                errno = 0;
                frac = pow(2, -frac);
                if (errno)
                    printf("Unknown\n");
                else
                {
                    if (frac >= 1.0 || frac == 0.0)
                        printf("%.1f\n", frac);
                    else
                        printf("1/%.0f\n", 1.0/frac);
                }
            }

            else if (tag == 0x9203)     /* Brightness */
            {
                if ((uint32_t)snum == 0xFFFFFFFF)
                    printf("Unknown\n");
                else
                    printf("%.2f\n", frac);
            }

            else if (tag == 0x9204)     /* Exposure Bias */
            {
                printf("%.2f step\n", frac);
            }

        }   /* end if rational */

    }   /* end for() */


    return exifoff - tiffoff;
}




/*
 *  read_tiff(fp, clen) - Read an entire Tiff block
 *
 *  Within APP1, lies a TIFF section and within that secton
 *  lies several number of IFDs (Exif, GPS). The TIFF header
 *  specified endianness too.
 *
 *  This function reads the TIFF section and dumps data if
 *  g_verbose is set. Otherwise just grabs res_[x,y,unit]
 *
 *  Uses g_buf
 *
 *  Returns: number of bytes absorbed from input if ok
 *           0 if corrupt
 */

static int read_tiff(FILE *fp, int clen)
{
    int natts, tiffoff, i, isle, nfields, bhop;

    natts = tiffoff = 0;

    /* At the TIFF header.
      Have 2 byte endianness, 0x002A, 4 byte IFD offset */

    if (clen < 8)
        return 0;

    if (fread(g_buf, 1, 8, fp) != 8)
        return 0;

    clen -= 8; tiffoff += 8;

    if (g_buf[0] == 0x49 && g_buf[1] == 0x49)       /* Little endian */
        isle = 1;
    else if (g_buf[0] == 0x4D && g_buf[1] == 0x4D)  /* Big endian */
        isle = 0;
    else
        return 0;

    if (str_to_word(g_buf + 2, isle) != 0x2A)       /* Check word */
        return 0;

    bhop = str_to_dword(g_buf + 4, isle);           /* IFD offset   */
    bhop -= 8;                                      /* From 0 point */

    if (bhop > clen)
        return 0;

    if (!fp_move_forward(fp, bhop))
        return 0;

    clen -= bhop; tiffoff += bhop;


    /* At the TIFF IFD. I hope...
       2 byte count, then fields then 4 byte pointer to next IFD */

    if (clen < 6)
        return 0;

    if (fread(g_buf, 1, 2, fp) != 2)
        return 0;

    clen -=2; tiffoff += 2;

    nfields = str_to_word(g_buf, isle);

    if (clen < nfields * 12 + 4)
        return 0;

    if (g_verbose)
    {
        printf("\n    Image (EXIF)\n");
        printf(  "    ------------\n");
    }


    /* Read fields. Just like exif */

    for (i = 0; i < nfields; i++)
    {
        unsigned int tag, cnt, val;

        if (fread(g_buf, 1, 12, fp) != 12)
            return 0;

        clen -= 12; tiffoff += 12;

        tag = str_to_word(g_buf + 0, isle);
        cnt = str_to_dword(g_buf + 4, isle);
        val = str_to_dword(g_buf + 8, isle);

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
                    char *str = (char *)g_buf + 8;

                    g_buf[11] = 0;        /* Just in case */

                    printf("    %s: ", getifdname(tag, 1));

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
                    break;
                }

                /* else we fall through to next group */

            case 0x011A:        /* Xresolution */
            case 0x011B:        /* Y resolution */
            case 0x0132:        /* DateTime */
            case 0x8769:        /* Exif IFD Offset */
            case 0x8825:        /* GPS IFD Offset */

                if (natts >= NELEMS(g_ifdatt))
                    return 0;

                g_ifdatt[natts].name = getifdname(tag, 1);
                g_ifdatt[natts].tag = tag;
                g_ifdatt[natts].cnt = cnt;
                g_ifdatt[natts].off = val;
                natts++;

            break;


            case 0x0128:        /* ResolutionUnit */

                /* Subtract one to be same as JFIF. Honestly... */

                jpginf.res_unit = str_to_word(g_buf + 8, isle) - 1;

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

    qsort(g_ifdatt, natts, sizeof(g_ifdatt[0]), ifdcompare);


    /* Scan the rest of the Tiff IFD block */

    for (i = 0; i < natts; i++)
    {
        /* Move to the right offset. Currently at: tiffoff */

        bhop = g_ifdatt[i].off - tiffoff;
        if (bhop > clen)
            return 0;

        if (!fp_move_forward(fp, bhop))
            return 0;

        clen -= bhop; tiffoff += bhop;

        if (tiffoff != g_ifdatt[i].off)     /* Again, just in case */
            return 0;


        if (g_verbose && g_ifdatt[i].tag == 0x8769)
        {
            /* An entire EXIF IFD block, somewhere up further */

            int ret = read_ifd_exif(fp, clen, tiffoff, isle);
            if (!ret || ret > clen)
                return 0;

            clen -= ret;
            tiffoff += ret;
        }

        else if (g_verbose && g_ifdatt[i].tag == 0x8825)
        {
            /* GPS IFD offset. Not processed at the moment */

        }

        else if (g_ifdatt[i].tag == 0x011A || g_ifdatt[i].tag == 0x011B)
        {
            /* Image resolution, x and y */
            /* We have two longs in the form of a fraction */

            uint32_t num, den;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(g_buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; tiffoff += 8;

            num = str_to_dword(g_buf + 0, isle);
            den = str_to_dword(g_buf + 4, isle);

            if (den == 0)
                den = 1;

            frac = (double)num/(double)den;

            if (g_verbose)
                printf("    %s: %0.f\n", g_ifdatt[i].name, frac);

            if (g_ifdatt[i].tag == 0x011A)
                jpginf.res_x = (int)(frac + 0.5);      /* Round properly */
            else
                jpginf.res_y = (int)(frac + 0.5);
        }

        else if (g_verbose)
        {
            /* ASCII: cut and ... paste ... :-)

               string of length g_ifdatt[i].cnt including terminator.
               This string can be terminated early (generally to stop
               having to pack it into bytes inside the value)
            */

            int ch, k;

            if (clen < g_ifdatt[i].cnt)
                return 0;

            printf("    %s: ", g_ifdatt[i].name);

            for (k = 0; k < g_ifdatt[i].cnt; k++)
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

            if (k >= g_ifdatt[i].cnt)
                return 0;       /* Unterminated */
        }

    }   /* end for() */

    return tiffoff;
}




/*
 *  read_app1(fp, clen) - Read the jpg app1 segment, EXIF
 *
 *  This reads and dumps all information in block. Hope to
 *  gather res_x, res_y and res_unit at the very least
 *
 *  Function absorbs all bytes passed in clen
 *  (reads entire segment)
 *
 *  Uses g_buf
 *
 *  Returns: 1 if ok, 0 if corrupt
 */

static int read_app1(FILE *fp, int clen)
{
    int ret;

    if (clen < 14)
        return 0;

    /* APP1 "EXIF\0\0" ident is immediately followed
       by a TIFF Header and one or two IFDs */

    if (fread(g_buf, 1, 6, fp) != 6)
        return 0;

    clen -= 6;

    if (g_buf[4] != 0 || g_buf[5] != 0)
        return 0;

    if (strcmp((char *)g_buf, "Exif") != 0)
        return 0;

    /* Positioned at the beginning of the TIFF header */

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
 *  Uses g_buf
 *
 *  Returns: 1 if ok, 0 if corrupt
 */

static int read_app0(FILE *fp, int clen)
{
    if (clen < 5)
        return 0;

    if (fread(g_buf, 1, 5, fp) != 5)
        return 0;

    clen -= 5;

    if (g_buf[4] != 0)
        return 0;

    if (strcmp((char *)g_buf, "JFIF") != 0)
    {
        /* Not the end of the world. Can have
           a JFXX header instead. Skip it. Bah... */

        return fp_move_forward(fp, clen);
    }

    if (clen < 7)
        return 0;

    if (fread(g_buf, 1, 7, fp) != 7)
        return 0;

    clen -= 7;

    jpginf.den_unit = g_buf[2];
    jpginf.den_x = strbe_to_word(g_buf + 3);
    jpginf.den_y = strbe_to_word(g_buf + 5);

    if (g_verbose)
    {
        printf("\n    Image (JFIF)\n");
        printf(  "    ------------\n");

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
 *  Uses g_buf
 *
 *  Returns: 1 if ok, 0 if end of JPG, -1 if corrupt
 */

static int read_marker(FILE *fp)
{
    int clen, ret;

    if (fread(g_buf, 1, 2, fp) != 2)
        return -1;

    if (g_buf[0] != 0xFF)
        return -1;

    while (g_buf[1] == 0xFF)
    {
        /* 0xFF is fill padding, keep chucking it away... */

        if ((ret = getc(fp)) < 0)
            return -1;
        g_buf[1] = ret;
    }

    if (g_buf[1] == 0xD9 ||     /* EOI */
        g_buf[1] == 0xDA  )     /* SOS */
    {
        /* end of image or start of scan. We're done */
        return 0;
    }


    /* Read the 2 byte content length, big endian */

    if (fread(g_buf + 2, 1, 2, fp) != 2)
        return -1;

    clen = strbe_to_word(g_buf + 2) - 2;

    if (clen < 0)
        return -1;

    ret = 1;    /* marker is OK */

    if (g_buf[1] >= 0xC0 && g_buf[1] <= 0xC3)       /* SOF */
    {
        ret = read_sofx(fp, clen);
    }
    else if (g_buf[1] == 0xe0)                      /* APP0 */
    {
        ret = read_app0(fp, clen);
    }
    else if (g_buf[1] == 0xe1)                      /* APP1 */
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

int process_image_jpg(const char *fname, int isverbose)
{
    unsigned char jpgsoi[2];
    FILE *fp;
    int ret;

    g_verbose = isverbose;

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
        ret = read_marker(fp);

        if (ret < 0)
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


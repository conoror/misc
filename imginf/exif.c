/*
 *  Exif and GPS information within a TIFF jpeg block
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
#include <ctype.h>
#include <math.h>
#include <errno.h>

#include "imginf.h"




/*
 *  read_ifd_exif(fp, clen) - Read an EXIF Tiff IFD section
 *
 *  Within APP1, lies a TIFF section and within that section
 *  lies a number of IFDs, one of which is an Exif IFD.
 *  This function reads that section and dumps data (does
 *  not read g_verbose, check that before calling routine!
 *
 *  All IFD entry references are relative to the very start of
 *  the TIFF header. The entries are stored on the stack as all
 *  other IFDs uses the same structure (i.e. can't be global)
 *
 *  Returns: number of bytes absorbed from input if ok
 *           0 if corrupt
 */

int read_ifd_exif(FILE *fp, int clen, int tiffoff, int isle)
{
    unsigned char buf[20];
    struct ifdentry ifdent[20];         /* 400 bytes */
    int exifoff, nents, nfields, i;

    nents = 0;              /* Offset in ifdent[] */
    exifoff = tiffoff;      /* Offset rel to start of tiff */


    /* At the EXIF IFD. 2 byte count, then count number
       of fields then a 4 byte pointer to next IFD */

    if (clen < 6)
        return 0;

    if (fread(buf, 1, 2, fp) != 2)
        return 0;

    clen -=2; exifoff += 2;

    nfields = str_to_word(buf, isle);

    if (clen < nfields * 12 + 4)
        return 0;


    printf("\n"
           "    Camera\n"
           "    ------\n");


    /* Read fields */

    for (i = 0; i < nfields; i++)
    {
        unsigned int tag, type, cnt, dval, wval;

        if (fread(buf, 1, 12, fp) != 12)
            return 0;

        clen -= 12; exifoff += 12;

        tag = str_to_word(buf + 0, isle);
        type = str_to_word(buf + 2, isle);

        cnt = str_to_dword(buf + 4, isle);
        dval = str_to_dword(buf + 8, isle);
        wval = str_to_word(buf + 8, isle);

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

                if (nents >= NELEMS(ifdent))
                {
                    printf("Internal error, out of space in exif parser\n");
                    return 0;
                }

                ifdent[nents].name = getifdname(tag, IFD_TN_EXIF);
                ifdent[nents].tag  = tag;
                ifdent[nents].type = type;
                ifdent[nents].cnt  = cnt;
                ifdent[nents].off  = dval;
                nents++;

            break;


            case 0xA001:
                printf("    Color space: ");
                if (wval == 1)
                    printf("sRGB\n");
                else if (wval == 0xFFFF)
                    printf("Uncalibrated\n");
                else
                    printf("Reserved\n");
            break;


            case 0x8822:
                printf("    Exposure program: ");
                switch (wval)
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


            case 0x8827:
                printf("    Speed rating: ISO-%u\n", wval);
            break;


            case 0x9207:
                printf("    Metering mode: ");
                switch (wval)
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


            case 0x9208:
                printf("    Light source: ");
                switch (wval)
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


            case 0x9209:
                printf("    Flash: ");
                switch (wval)
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


            case 0xA402:
                printf("    Exposure mode: ");
                switch (wval)
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


            case 0xA403:
                printf("    White balance: ");
                switch (wval)
                {
                    case 0: printf("Auto white balance\n");
                            break;
                    case 1: printf("Manual white balance\n");
                            break;
                    default:
                            printf("Reserved\n");
                }
            break;


            case 0xA406:
                printf("    Scene capture type: ");
                switch (wval)
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


            case 0xA408:
                printf("    Contrast: ");
                switch (wval)
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


            case 0xA409:
                printf("    Saturation: ");
                switch (wval)
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


            case 0xA40A:
                printf("    Sharpness: ");
                switch (wval)
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


            case 0xA40C:
                printf("    Subject distance range: ");
                switch (wval)
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
     *  sort tags by the offset they appear at. The file is
     *  parsed linearly - we don't go backwards...
     */

    qsort(ifdent, nents, sizeof(ifdent[0]), ifdcompare);


    /* Scan the rest of the IFD block and dump the rest of the info */

    for (i = 0; i < nents; i++)
    {
        int bhop;

        /* Move to the given offset. Currently at: exifoff */

        bhop = ifdent[i].off - exifoff;
        if (bhop > clen)
            return 0;

        if (!fp_move_forward(fp, bhop))
            return 0;

        clen -= bhop; exifoff += bhop;

        if (exifoff != ifdent[i].off)       /* Just in case! */
            return 0;


        if (ifdent[i].type == 2)
        {
            /* ASCII: 0x9003, 0xA420 tags

               string of length ifdent[i].cnt including terminator.
               This string can be terminated early (generally to stop
               having to pack it into bytes inside the dword value)
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

            if (k >= ifdent[i].cnt)
                return 0;       /* Unterminated */
        }


        else if (ifdent[i].type == 5)
        {
            /* Rational: Two ulongs in a fraction. I only
               use those for which .cnt is 1 and I attempt not
               to do stupid things like divide by zero! */

            uint32_t num, den, tag;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; exifoff += 8;

            num = str_to_dword(buf + 0, isle);
            den = str_to_dword(buf + 4, isle);
            tag = ifdent[i].tag;

            printf("    %s: ", ifdent[i].name);

            if (den == 0)
                den = 1;        /* Yes, I've seen this done... */

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

                else if (num == 0)
                    printf("Unknown\n");

                else
                    printf("%.2f\n", frac);
            }

            else
            {
                printf("%.1f\n", frac);
            }
        }


        else if (ifdent[i].type == 10)
        {
            /* Srational: Two slongs in a fraction.
               Only: 0x9201, 0x9203, 0x9204 */

            int32_t snum, sden;
            unsigned int tag;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; exifoff += 8;

            snum = (int32_t)str_to_dword(buf + 0, isle);
            sden = (int32_t)str_to_dword(buf + 4, isle);
            tag  = ifdent[i].tag;

            printf("    %s: ", ifdent[i].name);

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

        }

    }   /* end for() */


    return exifoff - tiffoff;
}




/*
 *  read_ifd_gps(fp, clen) - Read a GPS IFD section
 *
 *  The Tiff section may point to a GPS IFD. This function reads
 *  that section and dumps data (does not use g_verbose).
 *  As per read_ifd_exif for the most part...
 *
 *  Returns: number of bytes absorbed from input if ok
 *           0 if corrupt
 */

int read_ifd_gps(FILE *fp, int clen, int tiffoff, int isle)
{
    unsigned char buf[20];
    struct ifdentry ifdent[10];
    int gpsoff, nents, nfields, i;

    /* Reference defaults. None for lat/lon */

    int ref_lat   = '?';
    int ref_lon   = '?';
    int ref_alt   = 0;
    int ref_speed = 'K';
    int ref_track = 'T';
    int ref_imgdir = 'T';

    nents = 0;              /* Offset in ifdent[] */
    gpsoff = tiffoff;       /* Offset rel to start of tiff */


    /* At the GPS IFD. 2 byte count, then count number
       of fields then a 4 byte pointer to next IFD */

    if (clen < 6)
        return 0;

    if (fread(buf, 1, 2, fp) != 2)
        return 0;

    clen -=2; gpsoff += 2;

    nfields = str_to_word(buf, isle);

    if (clen < nfields * 12 + 4)
        return 0;


    printf("\n"
           "    GPS Data\n"
           "    --------\n");


    /* Read fields */

    for (i = 0; i < nfields; i++)
    {
        unsigned int tag, type, cnt, dval;

        if (fread(buf, 1, 12, fp) != 12)
            return 0;

        clen -= 12; gpsoff += 12;

        tag = str_to_word(buf + 0, isle);
        type = str_to_word(buf + 2, isle);

        cnt = str_to_dword(buf + 4, isle);
        dval = str_to_dword(buf + 8, isle);

        switch (tag)
        {
            case 0x02:      /* GPSLatitude */
            case 0x04:      /* GPSLongitude */
            case 0x06:      /* GPSAltitude */
            case 0x07:      /* GPSTimeStamp */
            case 0x0D:      /* GPSSpeed */
            case 0x0F:      /* GPSTrack */
            case 0x11:      /* GPSImgDirection */
            case 0x1D:      /* GPSDateStamp */

                if (nents >= NELEMS(ifdent))
                {
                    printf("Internal error, out of space in gps parser\n");
                    return 0;
                }

                ifdent[nents].name = getifdname(tag, IFD_TN_GPS);
                ifdent[nents].tag  = tag;
                ifdent[nents].type = type;
                ifdent[nents].cnt  = cnt;
                ifdent[nents].off  = dval;
                nents++;

            break;


            case 0x00:      /* GPSVersionID */
                printf("    GPS Version ID: %u.%u.%u.%u\n",
                        buf[8], buf[9], buf[10], buf[11]);
            break;


            case 0x01:      /* GPSLatitudeRef */
                ref_lat = buf[8];
            break;


            case 0x03:      /* GPSLongitudeRef */
                ref_lon = buf[8];
            break;


            case 0x05:      /* GPSAltitudeRef */
                ref_alt = buf[8];
            break;


            case 0x0C:      /* GPSSpeedRef */
                ref_speed = buf[8];
            break;


            case 0x0E:      /* GPSTrackRef */
                ref_track = buf[8];
            break;


            case 0x10:      /* GPSImgDirectionRef */
                ref_imgdir = buf[8];
            break;


        }   /* end switch */

    } /* End for */


    /* sort tags by their offset */

    qsort(ifdent, nents, sizeof(ifdent[0]), ifdcompare);


    /* Scan IFD blocks and dump info */

    for (i = 0; i < nents; i++)
    {
        int bhop;

        /* Move to the given offset. Currently at: gpsoff */

        bhop = ifdent[i].off - gpsoff;
        if (bhop > clen)
            return 0;

        if (!fp_move_forward(fp, bhop))
            return 0;

        clen -= bhop; gpsoff += bhop;

        if (gpsoff != ifdent[i].off)       /* Just in case! */
            return 0;


        if (ifdent[i].type == 2)
        {
            /* ASCII: 0x1D date stamp tag

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

                clen--; gpsoff++;

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


        else if (ifdent[i].type == 5 &&
                  (ifdent[i].tag == 0x02 ||     /* GPSLatitude */
                   ifdent[i].tag == 0x04 ||     /* GPSLongitude */
                   ifdent[i].tag == 0x07))      /* GPSTimeStamp */
        {
            /* Rational: Two ulongs in a fraction. There are
               3 of these for lat, long and timestamp */

            int k;
            uint32_t num[3], den[3];
            double frac[3];

            if (clen < 8 * 3)
                return 0;

            for (k = 0; k < 3; k++)
            {
                if (fread(buf, 1, 8, fp) != 8)
                    return 0;

                clen -= 8; gpsoff += 8;

                num[k] = str_to_dword(buf + 0, isle);
                den[k] = str_to_dword(buf + 4, isle);
                if (den[k] == 0)
                    den[k] = 1;

                frac[k] = (double)num[k]/(double)den[k];
            }

            printf("    %s: ", ifdent[i].name);

            if (ifdent[i].tag == 0x07)
            {
                /* Time stamp */
                printf("%.0f:%.0f:%.2f\n", frac[0], frac[1], frac[2]);
            }
            else
            {
                /* Lat/lon */

                printf("%.0f", frac[0]);

                if (den[1] == 1)
                {
                    /* DD MM SS */
                    printf(" %.0f %.2f", frac[1], frac[2]);
                }
                else
                {
                    /* DD MM.MMMM [sss?] */

                    if (den[1] == 10)
                        printf(" %.1f", frac[1]);
                    else if (den[1] == 100)
                        printf(" %.2f", frac[1]);
                    else if (den[1] == 1000)
                        printf(" %.3f", frac[1]);
                    else
                        printf(" %.4f", frac[1]);

                    if (num[2] != 0)
                        printf("%.2f", frac[2]);
                }

                if (ifdent[i].tag == 0x02)
                    printf(" %c\n", ref_lat);
                else
                    printf(" %c\n", ref_lon);
            }
        }


        else if (ifdent[i].type == 5)
        {
            /* Rational: Two ulongs in a fraction. One of these */

            uint32_t num, den;
            double frac;

            if (clen < 8)
                return 0;

            if (fread(buf, 1, 8, fp) != 8)
                return 0;

            clen -= 8; gpsoff += 8;

            num = str_to_dword(buf + 0, isle);
            den = str_to_dword(buf + 4, isle);
            if (den == 0)
                den = 1;

            frac = (double)num/(double)den;

            printf("    %s: ", ifdent[i].name);

            switch (ifdent[i].tag)
            {
                case 0x06:      /* GPSAltitude */
                    if (ref_alt)
                        printf("-");
                    printf("%.1f m\n", frac);
                break;

                case 0x0D:      /* GPSSpeed */
                    printf("%.1f", frac);

                    if (ref_speed == 'K' || ref_speed == 'k')
                        printf(" kph");
                    else if (ref_speed == 'M' || ref_speed == 'm')
                        printf(" mph");
                    else if (ref_speed == 'N' || ref_speed == 'n')
                        printf(" knots");

                    printf("\n");
                break;

                case 0x0F:      /* GPSTrack */
                    printf("%.0f", frac);

                    if (ref_track == 'T' || ref_track == 't')
                        printf(" (deg true)");
                    else if (ref_track == 'M' || ref_track == 'm')
                        printf(" (deg magnetic)");

                    printf("\n");
                break;

                case 0x11:      /* GPSImgDirection */
                    printf("%.0f", frac);

                    if (ref_imgdir == 'T' || ref_imgdir == 't')
                        printf(" (deg true)");
                    else if (ref_imgdir == 'M' || ref_imgdir == 'm')
                        printf(" (deg magnetic)");

                    printf("\n");
                break;
            }
        }

    }   /* end for() */


    return gpsoff - tiffoff;
}



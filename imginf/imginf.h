/*
 *  imginf.h
 */


#ifndef NELEMS
#define NELEMS(a)  (sizeof(a) / sizeof((a)[0]))
#endif


#define IFD_TN_TIFF 0
#define IFD_TN_EXIF 1
#define IFD_TN_GPS  2


struct ifdentry
{
    const char  *name;
    unsigned int tag;
    unsigned int type;
    unsigned int cnt;
    unsigned int off;
};


/*
 *  Some handy inlines for string to word/dword conversion
 *  http://www.greenend.org.uk/rjk/tech/inline.html
 */

extern __inline__ unsigned int strbe_to_word(const unsigned char *str)
{
    return str[1] | str[0]<<8;
}

extern __inline__ unsigned int strle_to_word(const unsigned char *str)
{
    return str[0] | str[1]<<8;
}

extern __inline__ unsigned int str_to_word(const unsigned char *str, int isle)
{
    if (isle)
        return str[0] | str[1]<<8;
    return str[1] | str[0]<<8;
}

extern __inline__ unsigned int str_to_dword(const unsigned char *str, int isle)
{
    if (isle)
        return str[0] | str[1]<<8 | str[2]<<16 | str[3]<<24;
    return str[3] | str[2]<<8 | str[1]<<16 | str[0]<<24;
}


extern int g_verbose;


extern int ifdcompare(const void *a, const void *b);
extern const char *getifdname(unsigned int tag, int tntype);
extern int fp_move_forward(FILE *fp, int hop);

extern int read_ifd_exif(FILE *fp, int clen, int tiffoff, int isle);
extern int read_ifd_gps(FILE *fp, int clen, int tiffoff, int isle);

extern int process_image_png(const char *fname);
extern int process_image_jpg(const char *fname);


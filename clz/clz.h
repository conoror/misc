/*
 *  clz.h - Conor's Lil' zip header file
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */

#define clz_destroy(ptr)                \
    do {                                \
        if (ptr)                        \
            clz_destroy_direct(ptr);    \
        ptr = 0;                        \
    } while(0)


extern void *clz_create(void);

extern int clz_setcb_get(void *aptr, size_t (*getfn)(void *, unsigned char **),
                         void *getpar, int usemem);

extern int clz_setcb_put(void *aptr, size_t (*putfn)(void *, void *, size_t),
                         void *putpar);

extern void clz_destroy_direct(void *aptr);
extern int clz_decompress(void *aptr, int *cbusedp, unsigned int *crc32p);

/* vi:set ts=4 sw=4 expandtab: */


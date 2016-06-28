/*
 *  clzinflate - Conor's Lil' zip inflation
 *
 *  Distribution and use of this software are as per the terms of the
 *  Simplified BSD License (also known as the "2-Clause License")
 *
 *  Copyright 2016 Conor F. O'Rourke. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "crc32.h"
#include "clz.h"


#define CLZ_WINDOW_SIZE     32 * 1024
#define CLZ_MAXHUFFBITS     16

#define CLZ_MAXVALS_LL      288
#define CLZ_MAXVALS_DIS     32
#define CLZ_MAXVALS_CLS     19

#define CLZ_CODESOK_LL      286
#define CLZ_CODESOK_DIS     30

#define CLZ_ERR_NONE        0
#define CLZ_ERR_INTERNAL    1
#define CLZ_ERR_INPUT       2
#define CLZ_ERR_CORRUPT     3
#define CLZ_ERR_OUTPUT      4


typedef struct
{
    int bitslo, bitshi;

    int bl_count[CLZ_MAXHUFFBITS + 1];

    int  decvalids;     /* Number of valid codes  */
    int  decalloc;      /* The allocated size     */
    int *decode;        /* Allocated table        */

} Hufftbl;




/*
 *  Static lookup tables and data. These are filled in once when
 *  clz_create() is called and then remain unchanged.
 */

static unsigned char g_byterev[256];
static unsigned int g_bitmask[CLZ_MAXHUFFBITS + 1];

static Hufftbl g_fixed_htll, g_fixed_htdis;
static int g_fixed_htdecode[CLZ_MAXVALS_LL + CLZ_MAXVALS_DIS];

static struct
{
    unsigned char   lenbits[30];
    unsigned short  lenbase[30];

    unsigned char   disbits[30];
    unsigned short  disbase[30];

} g_htextra;

static int g_initbuilds_done;




/*
 *  State table to allow reentrant calls to the clz routines
 */

typedef struct
{
    int error;

    size_t (*getfn)(void *, unsigned char **);
    void *getpar;
    size_t getnbtot;            /* running byte total for get */

    unsigned char *getccbuf;    /* Use a caller created buffer */
    unsigned char *getccend;    /* End of cc buffer (one past) */

    size_t (*putfn)(void *, void *, size_t);
    void *putpar;
    uint32_t putcrc;            /* CRC32 value of all the puts */

    unsigned int breg;
    int nbits;

    unsigned char  *sw_buf;     /* 32K sliding window */
    int             sw_cpos;    /* Current write position in window */
    int             sw_filled;  /* Has buffer ever been filled? */

    Hufftbl *htll, *htdis, *htcls;

} clz_state;




/*
 *  Init functions to fill in the static lookup tables and data
 *  -----------------------------------------------------------
 */




/*
 *  ibuild_fixed_huff() - fixed literal-length, distance Huff tables
 *
 *  RFC 1951, section 3.2.6. Compression with fixed Huffman codes
 *
 *  As these trees never change, they only have to be done once
 *  at startup. Tables are stored on the static heap (so init all-zeros)
 */

static void ibuild_fixed_huff(void)
{
    Hufftbl *htll, *htdis;
    int i;

    htll  = &g_fixed_htll;
    htdis = &g_fixed_htdis;

    htll->decode   = g_fixed_htdecode;
    htll->decalloc = CLZ_MAXVALS_LL;

    htdis->decode   = &g_fixed_htdecode[htll->decalloc];
    htdis->decalloc = CLZ_MAXVALS_DIS;


    /* Start with the literal-length table. Each entry is
       reoffset by the previous bit length max count. Thus the
       table becomes contiguous from 0 to 287. The RFC mentions
       that 286, 287 will never occur in the compressed stream */

    htll->bl_count[7] = 24;     /* 7 bit: codes 0 to 23    */
    htll->bl_count[8] = 152;    /* 8 bit: codes 48 to 199  */
    htll->bl_count[9] = 112;    /* 9 bit: codes 400 to 511 */

    htll->decvalids = CLZ_CODESOK_LL;

    htll->bitslo = 7;
    htll->bitshi = 9;


    /*      Huff code       Array offs      Decodes to
            ---------       ----------      ----------
             0 to  23         0 to  23      256 to 279
            48 to 191        24 to 167        0 to 143
           192 to 199       168 to 175      280 to 287
           400 to 511       176 to 287      144 to 255
    */

    for (i = 0; i < 24; i++)
        htll->decode[i] = 256 + i;

    for (i = 0; i < 144; i++)
        htll->decode[24 + i] = i;

    for (i = 168; i < 176; i++)
        htll->decode[i] = i + 112;

    for (i = 176; i < CLZ_MAXVALS_LL; i++)
        htll->decode[i] = i - 32;


    /* Now the fixed distance tree which is just straight boring
       5 bit codes. RFC says 30, 31 will never occur */

    htdis->bl_count[5] = 32;
    htdis->decvalids = CLZ_CODESOK_DIS;
    htdis->bitslo = 5;
    htdis->bitshi = 5;

    for (i = 0; i < 32; i++)
        htdis->decode[i] = i;
}




/*
 *  ibuild_huff_extrabits() - Huffman "extra bits" tables
 *
 *  See RFC 1951, section 3.2.5, length and distance codes.
 *
 *  Lookup tables for any extra bits a particular Huffman code
 *  result demands. This is a static lookup table done once at
 *  startup. The maximum table length is 30.
 */

static void ibuild_huff_extrabits(void)
{
    int i, k;

    /* Bits tables first. Can't think of a fancy way to do this */

    for (i = 0; i < 8; i++)
    {
        g_htextra.lenbits[i] = 0;
        g_htextra.disbits[i] = 0;
    }

    for (i = 8, k = 0; i < 28; i++)
    {
        if (i % 4 == 0)
            k++;

        g_htextra.lenbits[i] = k;
    }

    for (i = 4, k = 0; i < 30; i++)
    {
        if (i % 2 == 0)
            k++;

        g_htextra.disbits[i] = k;
    }


    /* Now the base. The resulting value will be this base plus
       the value of the extra bits. The bits can be used to set the
       the base in this case as it's just incrementing by 1 if bit
       is 0, 2 if bit is 1 etc
    */

    for (i = 0, k = 3; i < 28; i++)
    {
        g_htextra.lenbase[i] = k;
        k += 1 << g_htextra.lenbits[i];
    }

    for (i = 0, k = 1; i < 30; i++)
    {
        g_htextra.disbase[i] = k;
        k += 1 << g_htextra.disbits[i];
    }

    /* And some exceptions to the lengths for some reason */

    g_htextra.lenbits[28] = 0;
    g_htextra.lenbase[28] = 258;
}




/*
 *  ibuild_other_tables() - Any other static lookup tables
 *
 *  g_byterev - Lookup to reverse bits in a byte.
 *  g_bitmask - Lookup to get an all-ones bit mask for x bits
 *
 *  These are static lookup tables done once at startup
 */

static void ibuild_other_tables(void)
{
    int i, j;

    for (i = 0; i < 256; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (i & (1 << j))
                g_byterev[i] |= 0x80 >> j;
        }
    }

    /* Build a masking table. 2^n - 1 */

    for (i = 1, j = 2; i <= CLZ_MAXHUFFBITS; i++)
    {
        g_bitmask[i] = j - 1;
        j *= 2;
    }
}




/*
 *  Bit fetch and manipulate functions
 *  ----------------------------------
 */




/*
 *  breg_needbits(statep, n) - Make breg have at least n bits available
 *
 *  This function reads input as necessary to fill breg to the point
 *  requested. It is only called by breg_fetch() which does the check
 *  on the number of bits.
 *
 *  Returns:  1 on success
 *            0 on input read failure (Nothing else is set)
 */

static int breg_needbits(clz_state *statep, size_t n)
{
    unsigned char c;
    int ret;

    while (n > statep->nbits)
    {
        if (!statep->getccbuf)
        {
            if ((ret = getc((FILE *)statep->getpar)) < 0)
                return 0;

            c = (unsigned char)ret;
        }
        else
        {
            if (statep->getccbuf == statep->getccend)
            {
                if (!statep->getfn)
                {
                    /* Buffer empty, no way to get more */
                    return 0;
                }

                ret = statep->getfn(statep->getpar, &statep->getccbuf);

                if (ret <= 0 || !statep->getccbuf)
                    return 0;

                statep->getccend = statep->getccbuf + ret;
            }

            c = *statep->getccbuf++;
        }

        statep->breg |= (unsigned int)c << statep->nbits;
        statep->nbits += 8;
        statep->getnbtot++;
    }

    return 1;
}




/*
 *  breg_fetch(statep, n) - Fetch out bits from the bitregister
 *
 *  Takes the low n bits of the bitregister, removing them from
 *  the register and returns that unsigned value.
 *
 *  On error: sets statep->error which must be checked on return
 */

static unsigned int breg_fetch(clz_state *statep, size_t n)
{
    unsigned int bits;

    if (n > CLZ_MAXHUFFBITS)
    {
        statep->error = CLZ_ERR_INTERNAL;
        return 0;
    }

    if (n == 0 || statep->error)
        return 0;

    if (n > statep->nbits && !breg_needbits(statep, n))
    {
        statep->error = CLZ_ERR_INPUT;
        return 0;
    }

    bits = statep->breg & g_bitmask[n];
    statep->breg >>= n;
    statep->nbits -= n;

    return bits;
}




/*
 *  breg_discard(statep, n) - Discard any bits in the bitregister
 *
 *  Returns: The number of bits thrown away
 */

static int breg_discard(clz_state *statep)
{
    int i;

    i = statep->nbits;
    statep->nbits = 0;
    statep->breg = 0;
    return i;
}




/*  Huffman tree creation and decoding functions
    --------------------------------------------

    I'm sure the reader understands Huffmans perfectly but it stretches
    my poor brain a little so I'm explaining it to myself in this comment!
    Yes, I do go on a bit but maybe someone else will find this useful!

    Refer to RFC 3.2.1 and 3.2.2 where all the fun stuff is. A Huffman
    code is a linear grouping for a particular bit length done in such
    a way that a parser can unambiguously parse the code. Let's say there
    are 24 7-bit codes, 152 8-bit codes and 112 9-bit codes (this is the
    "fixed" Huffman decompression table).

    You start with 7 bits of coded input and check for the range 0-23. That
    covers the 7-bit range. Easy. If it's 24 or higher, you need another
    bit. Left shifting that bit in means the code doubles (eg: 30 -> 60).

    So now you have 8 bits ranging from 48 - 199. If you've 200 or higher
    you need 9 bits.

    Shift again to get 9 bits ranging from 400 - 511. Above that is an
    invalid code sequence.

    Now I could just make a lookup table from 0 to 511 to get the decoded
    values (symbols as the RFC calls them) for any particular 7-9 bit
    Huffman code. However, you can eliminate all the gaps in that table
    just by subtracting the sum of the sizes of all the previous bit counts.

    It's actually a pretty minor optimisation for deflate but it's simple to
    do so why not do it:

      At 7 bits,  24 codes. Min code=0.   Subtract   0:  decode[0 - 23]
      To 8 bits, 152 codes. Min code=48.  Subtract  24:  decode[24 - 175]
      To 9 bits, 112 codes. Min code=352. Subtract 176:  decode[176 - 287]

    Clear enough. The decoded symbols can be placed in the table to be read
    out just by a code index. Where do the decoded symbols come from?

    The way a Huffman table is initially created is from a sequence of
    bits-lengths in the order of the symbols. We don't need the codes
    themselves - as shown above, 24 "7"s means codes 0-23. Then "8"
    means codes 48-... etc. Just a list of bit lengths will do.

    Not all symbols have to be used. If you have a "0" bit length, that
    symbol does not partipate. No Huffman code will give that symbol.

    To do all this for the fixed table you might have a sequence as:

      sequence  8 8 8 8 8 8 ...  8   9   9   9  ...  9   7   7   7  ...
      symbol    0 1 2 3 4 5 ... 143 144 145 146 ... 255 256 257 258 ...

    So 7 bits is the smallest and starts at code 0. Thus decode[0] = 256,
    decode[1] = 257 etc.

    But for the fixed table those values are given in section 3.2.6 so
    that list doesn't have to be wodged into the input. But other
    sequences are in there for "dynamic Huffman trees". For example:

      sequence:    3  3  3  3  2  2  0  2  2  2  0  0  4  4

      dec symbol:  0  1  2  3  4  5  6  7  8  9 10 11 12 13

      dec index:   5  6  7  8  0  1  -  2  3  4  -  -  9 10

    So a Huffman code of 1 (will be 2 bits) has a decode index equal to
    the code (1) and the decoded symbol is decode[1] or "5". A Huffman
    code of 5 (will be 3 bits) has a decoded symbol of decode[5] or "0".

    Creating decode[0-10] from the sequence is done simply by figuring
    out where each bit count starts in the array and then scanning the
    sequence.

    A code with a bit count of 2, starts at decode[0]. It has 5 entries,
    so a code with a bit count of 3 must start at decode[5]. It in turn
    has 4 entries so a code with a bit count of 4, starts at decode[9].

    Just make a count of bit lengths and create a list of offsets:

        bit length:  1  2  3  4  5  6  7  8 ...
        bl_count[]:  0  5  4  2  0  0  0  0 ...
        dtoffset[]:  -  0  5  9  -  -  -  -

    We don't care (x) about bit lengths of 0. If it occurs in the sequence
    we just skip the corresponding decoded value (that value won't have a
    Huffman code).

    So, when we run through the sequence, if a "3" occurs we store the
    corresponding decode value in decode[ tbloff[3] ] which is decode[5]
    to start with. Then increment tbloff[3] for the next "3" occurance
    which will drop into decode[6] etc. Nice and easy. Ish.

    I think that's over-explained at this stage so on with some code...
*/




/*
 *  huff_decode_input(statep, htree) - Decode input using Huffman tree
 *
 *  Fetches out enough bits from the input to get a code match in
 *  the Huffman tree htree. The Hufftbl structure stores the code
 *  length lo-hi ranges (for the fixed tree that is 7-9) so we start
 *  by reading that minimal amount of bits.
 *
 *  Note that Huffman codes are necessarily in reverse bit order as,
 *  to make longer codes, each new bit is left shifted into the LSB.
 *
 *  Returns:  The decoded value on success (value is >= 0)
 *            -1  on error and sets statep->error
 */

static int huff_decode_input(clz_state *statep, Hufftbl *htree)
{
    unsigned int hcode, bit;
    int n, decrange;

    n = htree->bitslo;

    hcode = breg_fetch(statep, n);
    if (statep->error)
        return -1;

    /* Reverse the bit order of hcode. Given that deflate uses
       a maximum of 16 bits, there isn't a lot of reason to
       accommodate more than that in compiled code.

       For 16 bits, shift hcode up to start at bit #15, then
       reverse each byte using a lookup table, and swap bytes:
            10  1111 0001 -> 1011 1100  0100 0000
                          -> 0000 0010  0011 1101
    */

#if (CLZ_MAXHUFFBITS <= 16)

    hcode <<= 16 - n;
    hcode = g_byterev[hcode >> 8] | (g_byterev[hcode & 0xFF] << 8);

#elif (CLZ_MAXHUFFBITS <= 24)

    hcode <<= 24 - n;
    hcode = (g_byterev[hcode >> 16])                 |
            (g_byterev[((hcode >> 8) & 0xFF)] << 8)  |
            (g_byterev[hcode & 0xFF] << 16);

#else
#error CLZ_MAXHUFFBITS is larger than 24 which is not catered for
#endif

    decrange = 0;

    while (n <= htree->bitshi)
    {
        decrange += htree->bl_count[n];

        if (hcode > htree->decvalids)
        {
            statep->error = CLZ_ERR_CORRUPT;
            return -1;
        }

        if (hcode < decrange)
            return htree->decode[hcode];

        /* Above range for that number of bits. Moar bits */

        bit = breg_fetch(statep, 1);
        if (statep->error)
            return -1;

        hcode = (hcode << 1) | bit;
        hcode -= decrange;
        n++;

    }

    statep->error = CLZ_ERR_INTERNAL;
    return -1;
}





/*
 *  cblseq_to_huff(cblenp, csize, htree) - code bit lengths to Huff-tree
 *
 *  RFC 1951, section 3.2.2 and the block comment above
 *
 *  Use a code bit-lengths sequence to create a Huffman tree. The tree
 *  is in the form of a lookup table matching incoming Huffman codes to
 *  an alphabet of decoded symbols. The space for this table is pre
 *  allocated to a limit given by htree->decalloc.
 *
 *  Returns:  1 on success
 *            0 if sequence exceed limits (Nothing else is set)
 */

static int cblseq_to_huff(const unsigned char *cblseq, int cblcnt,
                          Hufftbl *htree)
{
    int dtoffset[CLZ_MAXHUFFBITS + 1];
    int i, k;

    assert(cblcnt <= htree->decalloc);

    /* First zero the bl counts and decode table offsets */

    for (i = 0; i <= CLZ_MAXHUFFBITS; i++)
    {
        htree->bl_count[i] = 0;
        dtoffset[i] = 0;
    }


    /* Make a count of each occurrence of a code bit-length in the
       sequence. Then get the lo-hi ranges of the lengths (ignoring
       lengths of 0, which are in bl_count for, er, interest!?) */

    for (i = 0; i < cblcnt; i++)
    {
        if (cblseq[i] > CLZ_MAXHUFFBITS)
            return 0;

        htree->bl_count[ cblseq[i] ]++;
    }

    htree->bitslo = 0;

    for (i = 1; i <= CLZ_MAXHUFFBITS; i++)
    {
        if (htree->bl_count[i])
        {
            if (htree->bitslo == 0)
                htree->bitslo = i;

            htree->bitshi = i;
        }
    }


    /* Create the offsets into the decode table, for each code bit-length,
       that correspond to the initial Huffman code for that bit-length.
       Should end up with:
            bit length:  1  2  3  4  5  6  7  8 ...
            bl_count[]:  0  5  4  2  0  0  0  0 ...
            dtoffset[]:  -  0  5  9  -  -  -  -
    */

    for (i = htree->bitslo, k = 0; i <= htree->bitshi; i++)
    {
        dtoffset[i] = k;
        k += htree->bl_count[i];
    }

    /* Sum of all code bit-lengths equals the valid code range */

    htree->decvalids = k;


    /* Scan the cblseq to create the corresponding decoded symbols
       and place those symbols in the right bit-length area of the
       decode table. Skip zeros in the sequence. Off we go... */

    for (i = 0; i < cblcnt; i++)
    {
        int bc = cblseq[i];
        if (bc)
        {
            assert(dtoffset[bc] < htree->decvalids);

            htree->decode[dtoffset[bc]] = i;
            dtoffset[bc]++;
        }
    }

    /* And we're done */

    return 1;
}




/*
 *  huff_build_dynamic(statep) - Build a dynamic Huffman table
 *
 *  RFC 1951, section 3.2.7
 *
 *  Two dynamic Huffman trees follow an appropriate header in the input,
 *  defined by a sequence of code lengths. Those code lengths are in turn
 *  compressed with a Huffman code which I've called a htcls (Huffman tree
 *  for code length sequences) which is, of course, itself made up of a
 *  set of lengths read from the input. The input order of those lengths is
 *  not that of the actual order.
 *
 *  The dynamic trees are built when they are needed and stored in statep.
 *  The storage for those trees is built at clz_create() time.
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

static int huff_build_dynamic(clz_state *statep)
{
    static const unsigned char clsseqorder[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
        11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    Hufftbl *htll, *htdis, *htcls;
    unsigned int hlit, hdist, hclen;
    unsigned char cblseq[CLZ_MAXVALS_LL + CLZ_MAXVALS_DIS];
    int i, cblavail;


    htll  = statep->htll;       /* Literal length alphabet       */
    htdis = statep->htdis;      /* Distance alphabet             */
    htcls = statep->htcls;      /* Code length sequence alphabet */


    /* Read the code bit length counts from input, the variable names
       used are those that are used in RFC 1951. More comprehensible */

    hlit  = breg_fetch(statep, 5) + 257;    /* # htll:  257 - 286  */
    hdist = breg_fetch(statep, 5) + 1;      /* # htdis:   1 - 32   */
    hclen = breg_fetch(statep, 4) + 4;      /* # htcls:   4 - 19   */

    if (statep->error)
        return 0;

    if (hlit > 286 || hclen > 19)
    {
        statep->error = CLZ_ERR_CORRUPT;
        return 0;
    }



    /* The cblseq[] for htcls is read from input and there are hclen number
       of 3-bit (0-7) bitlengths. However because there might be a lot of
       zeros (unused corresponding alphabet symbols) the input is not in
       order of alphabet. So not something like "1 1 3 2 2 0 3 ...".

       The input hops about to fill the positions in cblseq. The hops are
       specified in clsseqorder[]. Thus the first bit-length read goes into
       cblseq[16], the second into cblseq[17]...etc. That means there may
       be plenty of 0 slots which are never filled (so we fill them first).

       The alphabet for codelengths is 19 long.
    */


    /* Zero all slots: */

    for (i = 0; i < 19; i++)
        cblseq[i] = 0;

    /* Read hclen number of fixed 3 bit out-of-order code lengths */

    for (i = 0; i < hclen; i++)
    {
        cblseq[ clsseqorder[i] ] = breg_fetch(statep, 3);
        if (statep->error)
            return 0;
    }

    /* Build the htcls Huffman tree: */

    if (!cblseq_to_huff(cblseq, 19, htcls))
    {
        statep->error = CLZ_ERR_CORRUPT;
        return 0;
    }


    /* Now the code lengths for both dynamic trees, htll and htdis, which
       are coded using the htcls Huffman tree as per section 3.2.7.
       Catch: these lengths are all in one stream and cross from lit-len
       to distance. Total count is up to 288 (ll) + 32 (dis).
    */

    i = 0;
    cblavail = hlit + hdist;

    while (cblavail)
    {
        int decsym, prevcbl, nreps, k;

        if ((decsym = huff_decode_input(statep, htcls)) < 0)
            return 0;

        /* The decoded symbol isn't the actual code length
           if it's >= 16. There's a table of repeats: */

        if (decsym < 16)
        {
            /* A literal code bit length */

            cblseq[i++] = decsym;
            nreps = 1;
        }

        else if (decsym == 16)
        {
            /* Copy previous entry 3-6 times (read another 2 bits) */

            nreps = breg_fetch(statep, 2) + 3;
            if (statep->error)
                return 0;

            if (i == 0 || nreps > cblavail)
            {
                statep->error = CLZ_ERR_CORRUPT;
                return 0;
            }

            prevcbl = cblseq[i - 1];
            for (k = 0; k < nreps; k++)
                cblseq[i++] = prevcbl;
        }

        else if (decsym == 17)
        {
            /* Put zeros for 3-10 times (read another 3 bits) */

            nreps = breg_fetch(statep, 3) + 3;
            if (statep->error)
                return 0;

            if (nreps > cblavail)
            {
                statep->error = CLZ_ERR_CORRUPT;
                return 0;
            }

            for (k = 0; k < nreps; k++)
                cblseq[i++] = 0;
        }

        else if (decsym == 18)
        {
            /* Put zeros for 11-138 times (read another 7 bits) */

            nreps = breg_fetch(statep, 7) + 11;
            if (statep->error)
                return 0;

            if (nreps > cblavail)
            {
                statep->error = CLZ_ERR_CORRUPT;
                return 0;
            }

            for (k = 0; k < nreps; k++)
                cblseq[i++] = 0;
        }

        else
        {
            statep->error = CLZ_ERR_CORRUPT;
            return 0;
        }

        cblavail -= nreps;

    }   /* while() */


    /* Have the code lengths, build the htll and htdis trees */

    if (!cblseq_to_huff(cblseq, hlit, htll))
    {
        statep->error = CLZ_ERR_CORRUPT;
        return 0;
    }

    if (!cblseq_to_huff(cblseq + hlit, hdist, htdis))
    {
        statep->error = CLZ_ERR_CORRUPT;
        return 0;
    }


    /* Post patch the number of valid decodes to allow for the
       RFC lit-len and distance input code restrictions */

    if (htll->decvalids > CLZ_CODESOK_LL)
        htll->decvalids = CLZ_CODESOK_LL;

    if (htdis->decvalids > CLZ_CODESOK_DIS)
        htdis->decvalids = CLZ_CODESOK_DIS;

    return 1;
}




/*
 *  Decompression support functions
 *  -------------------------------
 */




/*
 *  slwin_write(statep) - Write out the sliding window buffer
 *
 *  This function assumes that statep->sw_cpos is the amount
 *  of data to write out and zeros that value when complete.
 *  It also calls crc32 to keep a running CRC32 of the output.
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

static int slwin_write(clz_state *statep)
{
    size_t nbytes;

    if (!statep->sw_cpos)
        return 1;

    if (statep->putfn)
    {
        nbytes = statep->putfn(statep->putpar,
                               statep->sw_buf, statep->sw_cpos);
    }
    else
    {
        nbytes = fwrite(statep->sw_buf, 1,
                        statep->sw_cpos, (FILE *)statep->putpar);
    }

    if (nbytes != statep->sw_cpos)
    {
        statep->error = CLZ_ERR_OUTPUT;
        return 0;
    }

    statep->putcrc = crc32(statep->putcrc, statep->sw_buf, statep->sw_cpos);
    statep->sw_cpos = 0;

    return 1;
}




/*
 *  slwin_read(statep, nbytes) - Read into sliding window buffer
 *
 *  This is only called by process_block_stored() and used to place
 *  at most nbytes of data into the sliding window, subject to space
 *  available. This function looks a bit like breg_needbits() but reads
 *  multiple bytes instead of one. The mem copy is somewhat involved...
 *
 *  Returns:  the number of bytes placed on success (> 0)
 *            0 on error and sets statep->error
 */

static int slwin_read(clz_state *statep, size_t nbytes)
{
    size_t rdbytes, inbytes;

    rdbytes = CLZ_WINDOW_SIZE - (size_t)statep->sw_cpos;

    if (!nbytes || !rdbytes)
    {
        statep->error = CLZ_ERR_INTERNAL;
        return 0;
    }

    if (nbytes < rdbytes)
        rdbytes = nbytes;

    /* Read a max of rdbytes into sw_buf at sw_cpos */

    if (!statep->getccbuf)
    {
        inbytes = fread(statep->sw_buf + statep->sw_cpos, 1,
                        rdbytes, (FILE *)statep->getpar);

        if (inbytes != rdbytes)
        {
            statep->error = CLZ_ERR_INPUT;
            return 0;
        }
    }
    else
    {
        /* Rather than try and fill the sliding window with possible
           repeated calls to fill the buffer, I use as much as I can
           of the current buffer until it's empty and then return */

        if (statep->getccbuf == statep->getccend)
        {
            if (!statep->getfn)
            {
                statep->error = CLZ_ERR_INPUT;
                return 0;
            }

            inbytes = statep->getfn(statep->getpar, &statep->getccbuf);

            if (inbytes <= 0 || !statep->getccbuf)
            {
                statep->error = CLZ_ERR_INPUT;
                return 0;
            }

            statep->getccend = statep->getccbuf + inbytes;
        }

        if (rdbytes > statep->getccend - statep->getccbuf)
            rdbytes = statep->getccend - statep->getccbuf;

        memcpy(statep->sw_buf + statep->sw_cpos, statep->getccbuf, rdbytes);
        statep->getccbuf += rdbytes;
    }

    statep->sw_cpos += rdbytes;
    statep->getnbtot += rdbytes;
    return rdbytes;
}




/*
 *  Decompression and inflation functions
 *  -------------------------------------
 */




/*
 *  process_block_stored(statep) - Process uncompressed block
 *
 *  RFC 1951, section 3.2.4. Pretty straightforward
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

static int process_block_stored(clz_state *statep)
{
    unsigned int len, nlen;


    /* Discard up to next byte boundary */

    if (breg_discard(statep) >= 8)
    {
        statep->error = CLZ_ERR_INTERNAL;
        return 0;
    }


    /* First 2 16-bit words are length and its complement */

    len  = breg_fetch(statep, 16);
    nlen = breg_fetch(statep, 16);

    if (statep->error)
        return 0;

    if (len != (~nlen & 0xffff))
    {
        statep->error = CLZ_ERR_CORRUPT;
        return 0;
    }


    /* Copy len bytes of data to output. It is necessary to run input
       through the sliding window regardless - any future block can
       easily refer back to this one for a copy of data */

    while (len)
    {
        size_t nfilled = slwin_read(statep, len);

        if (!nfilled)
            return 0;

        assert(nfilled <= len && statep->sw_cpos <= CLZ_WINDOW_SIZE);

        len -= nfilled;

        if (statep->sw_cpos == CLZ_WINDOW_SIZE)
        {
            if (!slwin_write(statep))
                return 0;

            statep->sw_filled = 1;
        }
    }

    return 1;
}




/*
 *  inflate_block(statep) - Decompress a block
 *
 *  RFC 1951, section 3.2.3 and 3.2.5
 *
 *  Takes two huffman trees, a literal-length tree and a distance tree,
 *  and uses those trees to deflate the input stream into the sliding
 *  window, writing it whenever it fills.
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

int inflate_block(clz_state *statep, Hufftbl *htll, Hufftbl *htdis)
{
    int decsym, copylen, copydist;

    while (1)
    {
        /* The first decode from input is literal-length (htll) */

        if ((decsym = huff_decode_input(statep, htll)) < 0)
            return 0;

        /*
         *  Decoded symbol is: End-of-block marker
         */

        if (decsym == 256)
            break;


        /*
         *  Decoded symbol is: Literal byte
         */

        if (decsym < 256)
        {
            statep->sw_buf[statep->sw_cpos++] = decsym;

            if (statep->sw_cpos == CLZ_WINDOW_SIZE)
            {
                if (!slwin_write(statep))
                    return 0;

                statep->sw_filled = 1;
            }

            continue;
        }


        /*
         *  Decoded symbol is 257 to 285: copy length and distance
         */


        /* copy length: Use the g_htextra lookups to fetch extra bits
           and add those to a base length */

        decsym -= 257;

        copylen = breg_fetch(statep, g_htextra.lenbits[decsym]);
        if (statep->error)
            return 0;

        copylen += g_htextra.lenbase[decsym];


        /* The next decode from input is distance (htdis). Also
           fetch extra bits and add those to a base length: */

        if ((decsym = huff_decode_input(statep, htdis)) < 0)
            return 0;

        copydist = breg_fetch(statep, g_htextra.disbits[decsym]);
        if (statep->error)
            return 0;

        copydist += g_htextra.disbase[decsym];


        /* Have a length and a backward distance. This is backwards
           from 1 to 32768 into what is a sliding window of 32K
           so can end up with a negative start point (thus sw_cpos
           and copydist are both signed ints) */

        copydist = statep->sw_cpos - copydist;
        if (copydist < 0)
        {
            /* Distance refers beyond the start of the buffer, wrap it */

            copydist += CLZ_WINDOW_SIZE;

            if (!statep->sw_filled)
            {
                /* Buffer cannot be wrapped as it was never filled */
                statep->error = CLZ_ERR_CORRUPT;
                return 0;
            }
        }


        /* Copy across: This could probably be faster... */

        while (copylen--)
        {
            statep->sw_buf[statep->sw_cpos++] = statep->sw_buf[copydist++];

            if (copydist == CLZ_WINDOW_SIZE)
                copydist = 0;

            if (statep->sw_cpos == CLZ_WINDOW_SIZE)
            {
                if (!slwin_write(statep))
                    return 0;

                statep->sw_filled = 1;
            }
        }

    }   /* while (1) ... */

    return 1;
}




/*
 *  inflate_fixed(statep) - Decompress with fixed Huffman tree
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

int process_block_fixed(clz_state *statep)
{
    return inflate_block(statep, &g_fixed_htll, &g_fixed_htdis);
}




/*
 *  inflate_dynamic(statep) - Decompress with dynamic Huffman tree
 *
 *  Called huff_build_dynamic creates lit-len and distances trees
 *  in the pre-allocated store in the current state (htll, htdis)
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

int process_block_dynamic(clz_state *statep)
{
    if (!huff_build_dynamic(statep))
        return 0;

    return inflate_block(statep, statep->htll, statep->htdis);
}




/*
 *  decompress_input(statep) - Decompress input to output using statep
 *
 *  RFC 1951, section 3.2.3
 *
 *  This is the start of a new decompression sequence. The input should be
 *  a series of blocks, some of which may be Deflated and some of which may
 *  be uncompressed. The RFC notes that backward references (Deflate) may
 *  reach back to refer to a string in a previous block ...
 *
 *  Returns:  1 on success
 *            0 on error and sets statep->error
 */

static int decompress_input(clz_state *statep)
{
    int bfinal, btype;

    /* reset any state needed */

    statep->error = 0;

    statep->getnbtot = 0;
    statep->putcrc = 0;

    statep->breg = 0;
    statep->nbits = 0;

    statep->sw_cpos = 0;
    statep->sw_filled = 0;


    /* decompress a block at a time */

    do {

        bfinal = breg_fetch(statep, 1);
        btype = breg_fetch(statep, 2);

        if (statep->error)
            return 0;

        if (btype == 0)
        {
            /* Uncompressed */

            if (!process_block_stored(statep))
                return 0;
        }

        else if (btype == 1)
        {
            /* Deflate, fixed Huffman tree */

            if (!process_block_fixed(statep))
                return 0;
        }

        else if (btype == 2)
        {
            /* Deflate, dynamic Huffman tree */

            if (!process_block_dynamic(statep))
                return 0;
        }

        else
        {
            /* Error */

            statep->error = CLZ_ERR_CORRUPT;
            return 0;
        }


    } while (!bfinal);


    /* Write out any remaining pending output */

    if (!slwin_write(statep))
        return 0;

    return 1;
}




/*
 *  User callable clz_* functions
 *  -----------------------------
 */




/**
 *  clz_create() - Create state for subsequent clz calls
 *
 *  Before you call anything else, you must call clz_create()
 *  to create state and (if needed) initialise the static
 *  lookup tables.
 *
 *  On reception of the state pointer, use it to set data
 *  get and put function pointers if needed. The default
 *  read and write uses stdin and stdout otherwise.
 *
 *  On completion, call clz_destroy() to deallocate memory.
 *
 *  Returns:  Allocated state pointer as anonymous pointer
 *            NULL on error and sets errno
 */

void *clz_create(void)
{
    clz_state *statep;
    int htbytes;

    if ((statep = calloc(1, sizeof(clz_state))) == NULL)
        return 0;

    statep->getpar = stdin;
    statep->putpar = stdout;

    if ((statep->sw_buf = malloc(CLZ_WINDOW_SIZE)) == NULL)
    {
        free(statep);
        errno = ENOMEM;
        return 0;
    }

    htbytes =
        (3 * sizeof(Hufftbl)) +
        (CLZ_MAXVALS_LL + CLZ_MAXVALS_DIS + CLZ_MAXVALS_CLS) * sizeof(int);

    statep->htll = malloc(htbytes);
    if (statep->htll == NULL)
    {
        free(statep->sw_buf);
        free(statep);
        errno = ENOMEM;
        return 0;
    }

    statep->htdis = &statep->htll[1];
    statep->htcls = &statep->htll[2];

    /* Hufftbl starts with an int and must be aligned to that.
       Therefore using a Hufftbl *, the alignment is right */

    statep->htll->decode     = (int *)(&statep->htll[3]);
    statep->htll->decalloc   = CLZ_MAXVALS_LL;
    statep->htll->decvalids  = 0;

    statep->htdis->decode    = &statep->htll->decode[statep->htll->decalloc];
    statep->htdis->decalloc  = CLZ_MAXVALS_DIS;
    statep->htdis->decvalids = 0;

    statep->htcls->decode    = &statep->htdis->decode[statep->htdis->decalloc];
    statep->htcls->decalloc  = CLZ_MAXVALS_CLS;
    statep->htcls->decvalids = 0;


    /* Got here without problems so if initialisation of all the
       global static lookup tables needs to be, do that now */

    if (!g_initbuilds_done)
    {
        ibuild_fixed_huff();
        ibuild_huff_extrabits();
        ibuild_other_tables();

        g_initbuilds_done = 1;
    }

    /* Done */

    return (void *)statep;
}




/**
 *  clz_setcb_get(aptr, getfn, getpar, usemem) - Set get data callback
 *
 *  You may call this routine to set callback functions when the inflate
 *  routines need to get more data. If not called, stdin is the default
 *
 *  If usemem is zero, getpar must point to a FILE * and getfn is ignored.
 *
 *  If usemem is non-zero and getfn() is supplied, it is used to fill a
 *  user supplied buffer with as much input as getfn wishes to place in it.
 *  getfn() will be called with getpar (an optional parameter the caller can
 *  use to determine who is calling) and a pointer to a location to store
 *  the pointer to the user supplied buffer. It will return the number of
 *  bytes places in the buffer: nbytes = getfn(getpar, &getccbuf);
 *
 *  If usemem if non-zero and getfn() is NULL, getpar will point to a
 *  one off read buffer of size usemem. When that buffer is empty, there
 *  is no more input to read.
 *
 *  Returns:  1 on success
 *            0 on error and sets errno (to EINVAL)
 */

int clz_setcb_get(void *aptr, size_t (*getfn)(void *, unsigned char **),
                  void *getpar, int usemem)
{
    clz_state *statep;

    if (aptr == NULL)
    {
        errno = EINVAL;
        return 0;
    }

    statep = (clz_state *)aptr;

    statep->getfn = getfn;
    statep->getpar = getpar;

    if (!usemem)
    {
        if (!getpar)
        {
            errno = EINVAL;
            return 0;
        }
        statep->getccbuf = 0;
        statep->getccend = 0;
    }
    else if (getfn)
    {
        /* usemem true, getfn supplied - use it to fill buffers */

        statep->getccbuf = statep->sw_buf;  /* A handy pointer! */
        statep->getccend = statep->sw_buf;  /* But empty buffer */
    }
    else
    {
        /* usemem true, getfn null - one off buffer of size usemem */

        if (!getpar || usemem < 0)
        {
            errno = EINVAL;
            return 0;
        }
        statep->getccbuf = (unsigned char *)getpar;
        statep->getccend = getpar;
        statep->getccend += usemem;
    }

    return 1;
}




/**
 *  clz_setcb_put(aptr, putfn, putpar) - Set put data callback
 *
 *  You may call this routine to set callback functions when the inflate
 *  routines need to write out data. If not called, stdout is the default

 *    - a pointer to a put function and parameter:
 *          size_t (*putfn)(void *gptr, void *buf, size_t bytes);
 *          void *putpar;
 *
 *  putfn() is used to output data, up to 32K at a time. If putfn is
 *  not set, then putpar must point to a FILE *. putfn() shall return
 *  the number of bytes written which must be that requested, or 0
 *  on an error. Called like: putfn(putpar, localbuf, 200);
 *
 *  Returns:  1 on success
 *            0 on error and sets errno (to EINVAL)
 */

int clz_setcb_put(void *aptr, size_t (*putfn)(void *, void *, size_t),
                  void *putpar)
{
    clz_state *statep;

    if ( (aptr == NULL)      ||
         (!putfn && !putpar)  )
    {
        errno = EINVAL;
        return 0;
    }

    statep = (clz_state *)aptr;

    statep->putfn  = putfn;
    statep->putpar = putpar;
    return 1;
}




/*
 *  clz_destroy_direct(aptr) - Free clz state (anonymous pointer)
 *
 *  Tears down the state table created by clz_create()
 *  Call the clz_destroy define instead which will zero the pointer
 *  after freeing it (avoids use after free scenarios)
 */

void clz_destroy_direct(void *aptr)
{
    clz_state *statep;

    if (!aptr)
        return;

    statep = (clz_state *)aptr;

    free(statep->htll);
    free(statep->sw_buf);
    free(statep);
}




/**
 *  clz_decompress(aptr, cbusedp) - Decompress stream from get to put
 *
 *  Using the get and put callback functions (if set) this routine
 *  takes an input stream and decompresses it to an output stream.
 *  The total number of bytes read from input is returned.
 *
 *  Any provided fill buffers may be left half empty. As it may be
 *  useful to know how much was consumed *cbusedp, if supplied, will
 *  update to that value.
 *
 *  Returns:  total bytes read on success
 *            0 on error and sets errno
 */

int clz_decompress(void *aptr, int *cbusedp, unsigned int *crc32p)
{
    clz_state *statep = (clz_state *)aptr;

    if (aptr == NULL)
    {
        errno = EINVAL;
        return 0;
    }

    if (!decompress_input(statep))
    {
        if (!statep->error)
            statep->error = CLZ_ERR_INTERNAL;

        switch (statep->error)
        {
            case CLZ_ERR_INPUT:     errno = EIO;    break;
            case CLZ_ERR_OUTPUT:    errno = ERANGE; break;
            case CLZ_ERR_CORRUPT:   errno = EILSEQ; break;
            default:                errno = EPERM;
        }
    }

    if (statep->getccbuf)
    {
        if (cbusedp)
            *cbusedp = statep->getccend - statep->getccbuf;

        /* Invalidate the buffer: If getfn is supplied, it will
           be called on first read, if not it will fail until
           clz_setcb_get() is called to set up a new buffer.
           sw_buf is handy to make them equal but still valid: */

        statep->getccbuf = statep->sw_buf;
        statep->getccend = statep->sw_buf;
    }

    if (statep->error)
        return 0;

    if (crc32p)
        *crc32p = (unsigned int)statep->putcrc;

    return (int)statep->getnbtot;
}


/* vi:set ts=4 sw=4 expandtab: */

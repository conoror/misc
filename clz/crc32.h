/*
 *  cksum.h: CRC32 and MD5 checksumming routines
 */

#include <stdint.h>
#include <stdlib.h>

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);


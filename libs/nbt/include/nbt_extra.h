/* Defines extra functions that make working with NBT data sources easier,
 * but require extra libraries like zlib. */
#ifndef CHOWDER_NBT_EXTRA_H
#define CHOWDER_NBT_EXTRA_H

#include "nbt.h"

/* unpacks the given fd, decompressing it if it's gzip-compressed. 
 * returns 0 on success, or a negative value on error. */
int nbt_unpack_file(int fd, struct nbt **out);

#endif // CHOWDER_NBT_EXTRA_H

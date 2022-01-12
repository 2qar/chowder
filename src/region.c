#include "region.h"

/* FIXME: misleading name, frees region data but not the struct itself. idk */
void free_region(struct region *r) {
	for (int z = 0; z < 32; ++z)
		for (int x = 0; x < 32; ++x)
			if (r->chunks[z][x] != NULL)
				free_chunk(r->chunks[z][x]);
}
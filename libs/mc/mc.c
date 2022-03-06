#include "mc.h"

static int scale(int x, int factor)
{
	return x >= 0 ? x / factor : (x + 1) / factor - 1;
}

int mc_coord_to_region(int x)
{
	return scale(x, 512);
}

int mc_coord_to_chunk(int x)
{
	return scale(x, 16);
}

int mc_chunk_to_region(int x)
{
	return scale(x, 32);
}

int mc_localized_chunk(int x)
{
	return x > 0 ? x % 32 : (x + 1) % 32 - 1;
}

int mc_coord_to_localized_chunk(int x)
{
	return mc_localized_chunk(mc_coord_to_chunk(x));
}

#include "player.h"

#include <stdlib.h>

void player_free(struct player *p)
{
	free(p->textures);
	free(p);
}

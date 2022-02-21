#ifndef CHOWDER_PLAYER_H
#define CHOWDER_PLAYER_H
#include <stdint.h>

struct player {
	uint8_t uuid[16];
	char username[17];
	char *textures;

	double x;
	double y;
	double z;
};

void player_free(struct player *);

#endif

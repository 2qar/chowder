#ifndef CHOWDER_PLAYER_H
#define CHOWDER_PLAYER_H

#include "slot.h"

#include <stdint.h>

#define PLAYER_INVENTORY_SIZE 46

struct player {
	uint8_t uuid[16];
	char username[17];
	char *textures;

	double x;
	double y;
	double z;

	int8_t selected_slot; // 0-8
	struct slot inventory[PLAYER_INVENTORY_SIZE];
};

void player_free(struct player *);

#endif

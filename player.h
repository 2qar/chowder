#ifndef CHOWDER_PLAYER_H
#define CHOWDER_PLAYER_H

struct player {
	uint8_t uuid[16];
	char username[17];
	char *textures;
};

#endif

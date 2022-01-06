#ifndef CHOWDER_SLOT_H
#define CHOWDER_SLOT_H
#include <stdbool.h>
#include <stdint.h>

struct slot {
	bool present;
	int32_t item_id;
	int8_t item_count;
	struct nbt *nbt;
};

#endif // CHOWDER_SLOT_H

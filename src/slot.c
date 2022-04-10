#include "slot.h"

#include "nbt.h"

void slot_free(struct slot *slot)
{
	nbt_free(slot->nbt);
	free(slot);
}

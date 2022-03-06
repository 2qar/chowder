#ifndef CHOWDER_VIEW_H
#define CHOWDER_VIEW_H

#include <stdbool.h>

struct view {
	int x;
	int z;
	int size;
};

#define VIEW_CONTAINS(view, x1, z1) \
	(x1 >= view.x - view.size && z1 >= view.z - view.size && \
	 x1 <= view.x + view.size && z1 <= view.z + view.size)

#define VIEW_FOREACH(view, vx, vz) \
	for (vx = view.x - view.size; vx <= view.x + view.size; ++vx) \
		for (vz = view.z - view.size; vz <= view.z + view.size; ++vz)

#endif // CHOWDER_VIEW_H

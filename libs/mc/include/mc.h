/* Defines some functions for dealing with various Minecraftisms.
 * This could probably be split further into multiple files, but whateva
 */
#ifndef CHOWDER_MC_H
#define CHOWDER_MC_H

int mc_coord_to_region(int);
int mc_coord_to_chunk(int);
int mc_chunk_to_region(int);
/* returns a global chunk's coords relative to the region it's in.
 * ex: mc_localized_chunk(-65) = -1 */
int mc_localized_chunk(int);
int mc_coord_to_localized_chunk(int);

#endif // CHOWDER_MC_H

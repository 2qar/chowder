#ifndef CHOWDER_SERVER_H
#define CHOWDER_SERVER_H

#include <stdint.h>
#include <openssl/evp.h>

#include "conn.h"
#include "packet.h"
#include "world.h"
#include "include/hashmap.h"

struct conn *server_handshake(int sfd, struct packet *);
int server_initialize_play_state(struct conn *, struct world *, struct hashmap *block_table);
int server_play(struct conn *, struct world *);

#endif

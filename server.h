#ifndef CHOWDER_SERVER_H
#define CHOWDER_SERVER_H

#include <stdint.h>
#include <openssl/evp.h>

#include "conn.h"
#include "login.h"
#include "packet.h"
#include "world.h"
#include "include/hashmap.h"

struct conn *server_accept_connection(int sfd, struct packet *, struct world *, struct login_ctx *);
int server_play(struct conn *, struct world *);

#endif

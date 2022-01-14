#ifndef CHOWDER_SERVER_H
#define CHOWDER_SERVER_H

#include <stdint.h>
#include <openssl/evp.h>

#include "conn.h"
#include "login.h"
#include "packet.h"
#include "protocol.h"
#include "world.h"
#include "hashmap.h"

struct conn *server_accept_connection(int sfd, struct packet *, struct world *, struct login_ctx *);
int server_play(struct conn *, struct world *);
struct protocol_do_err server_send_messages(struct list *connections, struct list *messages);

#endif

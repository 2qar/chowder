#include <stdint.h>
#include <stdbool.h>

#include <openssl/evp.h>

#include "conn.h"
#include "packet.h"

int handshake(int);
int server_list_ping(int);
int login_start(int, char[]);
int encryption_request(int, size_t, const unsigned char *, uint8_t[4]);
int encryption_response(int, EVP_PKEY_CTX *, const uint8_t[4], uint8_t[16]);
int login_success(struct conn *, const char[36], const char[16]);

int ping(struct conn *, uint8_t[8]);
int pong(struct conn *, uint8_t[8]);

int join_game(struct conn *);
int window_items(struct conn *);
int spawn_position(struct conn *, uint16_t, uint16_t, uint16_t);
int chunk_data(struct conn *, int, int, bool);
int player_position_look(struct conn *, int *teleport_id);
int teleport_confirm(struct recv_packet *, int server_teleport_id);

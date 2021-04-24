#include <stdint.h>
#include <stdbool.h>

#include <openssl/evp.h>

#include "conn.h"
#include "packet.h"
#include "region.h"
#include "world.h"

int handshake(struct conn *);
int server_list_ping(struct conn *);
int login_start(struct conn *, char[]);
int encryption_request(struct conn *, size_t, const unsigned char *, uint8_t[4]);
int encryption_response(struct conn *, EVP_PKEY_CTX *, const uint8_t[4], uint8_t[16]);
int login_success(struct conn *, const char[36], const char[16]);
int ping(struct conn *, uint8_t id[8]);
int pong(struct conn *, uint8_t id[8]);

int join_game(struct conn *);
int client_settings(struct conn *);
int window_items(struct conn *);
int held_item_change_clientbound(struct conn *, uint8_t slot);
int spawn_position(struct conn *, uint16_t, uint16_t, uint16_t);
int chunk_data(struct conn *, const struct chunk *, int x, int y, bool full);

enum player_info_action {
	PLAYER_INFO_ADD_PLAYER,
	PLAYER_INFO_UPDATE_GAMEMODE,
	PLAYER_INFO_UPDATE_LATENCY,
	PLAYER_INFO_UPDATE_DISPLAY_NAME,
	PLAYER_INFO_REMOVE_PLAYER,
};
struct player_info_property {
	char *name;
	char *value;
};
struct player_info {
	char uuid[16];
	union {
		struct {
			char username[16];
			size_t properties_len;
			struct player_info_property *properties;
			int32_t gamemode;
			int32_t ping;
			bool has_display_name;
			/* TODO: display name */
		} add;
		int32_t new_gamemode;
		int32_t new_ping;
		struct {
			bool has;
			/* TODO: display name */
		} display_name;
	};
};
int player_info(struct conn *, enum player_info_action, size_t players, struct player_info *);

int player_position_look(struct conn *, int *teleport_id);
int teleport_confirm(struct packet *, int server_teleport_id);
int keep_alive_clientbound(struct conn *c, time_t *t, uint64_t *id);
int keep_alive_serverbound(struct packet *p, uint64_t id);
int player_block_placement(struct packet *, struct world *);

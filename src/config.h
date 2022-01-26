#ifndef CHOWDER_CONFIG_H
#define CHOWDER_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

struct server_properties {
	char *gamemode;
	bool enable_command_block;
	char *level_name;
	char *motd;
	bool pvp;
	char *difficulty;
	uint32_t network_compression_threshold;
	bool require_resource_pack;
	uint32_t max_tick_time;
	uint32_t max_players;
	bool online_mode;
	bool enable_status;
	bool allow_flight;
	uint32_t view_distance;
	char *server_ip;
	char *resource_pack_prompt;
	bool allow_nether;
	uint32_t server_port;
	bool sync_chunk_writes;
	uint32_t op_permission_level;
	char *resource_pack;
	uint32_t entity_broadcast_range_percentage;
	uint32_t player_idle_timeout;
	bool force_gamemode;
	uint32_t rate_limit;
	bool hardcore;
	bool white_list;
	bool broadcast_console_to_ops;
	bool spawn_npcs;
	bool spawn_animals;
	uint32_t function_permission_level;
	bool spawn_monsters;
	bool enforce_whitelist;
	char *resource_pack_sha1;
	uint32_t spawn_protection;
	uint32_t max_world_size;
};

struct server_properties server_properties;

enum config_err {
	CONFIG_OK,
	CONFIG_READ,
	CONFIG_INVALID,
};

enum config_err read_server_properties(const char *path);
/* initializes server_properties with it's default values and writes it out to
 * the given path, returning 0 on success or -1 on failure. */
int set_default_server_properties(const char *path);
void free_server_properties();

#endif // CHOWDER_CONFIG_H

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LINE_LEN 1024

struct server_properties server_properties;

enum cv_type {
	CV_NONE,
	CV_NUM,
	CV_BOOL,
	CV_STR,
	CV_ESTR,
};

struct estr_mapping {
	const char *name;
	int value;
};

struct config_value {
	enum cv_type type;
	const char *prop_name;
	void *prop_field;
	union {
		uint32_t default_num;
		bool default_bool;
		const char *default_str;
		// the first entry in the mapping will be used as the default.
		// this should point to a zero-init-terminated list of valid
		// values.
		struct estr_mapping *default_estr;
	} data;
};

static struct estr_mapping gamemode_mapping[] = {
	{ "survival", 0 },
	{ 0 },
};

static struct estr_mapping difficulty_mapping[] = {
	{ "easy", 1 }, { "peaceful", 0 }, { "normal", 2 }, { "hard", 3 }, { 0 },
};

#define CV_MAP(TYPE, NAME, PROP_NAME, FIELD, DEFAULT_VALUE)                    \
	{                                                                      \
		.type = TYPE, .prop_name = PROP_NAME,                          \
		.prop_field = &server_properties.NAME,                         \
		.data.FIELD = DEFAULT_VALUE                                    \
	}
#define CV_MAP_NUM(PROP_NAME, NAME, DEFAULT_VALUE)                             \
	CV_MAP(CV_NUM, NAME, PROP_NAME, default_num, DEFAULT_VALUE)
#define CV_MAP_BOOL(PROP_NAME, NAME, DEFAULT_VALUE)                            \
	CV_MAP(CV_BOOL, NAME, PROP_NAME, default_bool, DEFAULT_VALUE)
#define CV_MAP_STR(PROP_NAME, NAME, DEFAULT_VALUE)                             \
	CV_MAP(CV_STR, NAME, PROP_NAME, default_str, DEFAULT_VALUE)
#define CV_MAP_ESTR(PROP_NAME, NAME)                                           \
	CV_MAP(CV_ESTR, NAME, PROP_NAME, default_estr, NAME##_mapping)

static struct config_value config_mappings[] = {
	CV_MAP_ESTR("gamemode", gamemode),
	CV_MAP_BOOL("enable-command-block", enable_command_block, 0),
	CV_MAP_STR("level-name", level_name, "world"),
	CV_MAP_STR("motd", motd, "A Minecraft Server"),
	CV_MAP_BOOL("pvp", pvp, true),
	CV_MAP_ESTR("difficulty", difficulty),
	CV_MAP_NUM("network-compression-threshold",
		   network_compression_threshold, 256),
	CV_MAP_BOOL("require-resource-pack", require_resource_pack, false),
	CV_MAP_NUM("max-tick-time", max_tick_time, 60000),
	CV_MAP_NUM("max-players", max_players, 20),
	CV_MAP_BOOL("online-mode", online_mode, true),
	CV_MAP_BOOL("enable-status", enable_status, true),
	CV_MAP_BOOL("allow-flight", allow_flight, false),
	CV_MAP_NUM("view-distance", view_distance, 10),
	CV_MAP_STR("server-ip", server_ip, NULL),
	CV_MAP_STR("resource-pack-prompt", resource_pack_prompt, NULL),
	CV_MAP_BOOL("allow-nether", allow_nether, true),
	CV_MAP_NUM("server-port", server_port, 25565),
	CV_MAP_BOOL("sync-chunk-writes", sync_chunk_writes, true),
	CV_MAP_NUM("op-permission-level", op_permission_level, 4),
	CV_MAP_STR("resource-pack", resource_pack, NULL),
	CV_MAP_NUM("entity-broadcast-range-percentage",
		   entity_broadcast_range_percentage, 100),
	CV_MAP_NUM("player-idle-timeout", player_idle_timeout, 0),
	CV_MAP_BOOL("force-gamemode", force_gamemode, false),
	CV_MAP_NUM("rate-limit", rate_limit, 0),
	CV_MAP_BOOL("hardcore", hardcore, false),
	CV_MAP_BOOL("white-list", white_list, false),
	CV_MAP_BOOL("broadcast-console-to-ops", broadcast_console_to_ops, true),
	CV_MAP_BOOL("spawn-npcs", spawn_npcs, true),
	CV_MAP_BOOL("spawn-animals", spawn_animals, true),
	CV_MAP_NUM("function-permission-level", function_permission_level, 2),
	CV_MAP_BOOL("spawn-monsters", spawn_monsters, true),
	CV_MAP_BOOL("enforce-whitelist", enforce_whitelist, true),
	CV_MAP_STR("resource-pack-sha1", resource_pack_sha1, NULL),
	CV_MAP_NUM("spawn-protection", spawn_protection, 16),
	CV_MAP_NUM("max-world-size", max_world_size, 29999984),
	{ 0 },
};

static int process_estr(char *value_str, struct estr_mapping *mapping,
			uint32_t *out)
{
	struct estr_mapping *value = mapping;
	while (value->name != NULL && strcmp(value->name, value_str))
		++value;
	if (value->name == NULL)
		return -1;
	*out = mapping->value;
	return 0;
}

static int process_value(int line_num, char *line)
{
	int eq_idx = 0;
	while (line[eq_idx] != '\0' && line[eq_idx] != '=')
		++eq_idx;
	if (eq_idx == 0 && line[eq_idx] == '=') {
		fprintf(stderr, "server.properties: no name on line %d\n",
			line_num);
		return -1;
	}
	line[eq_idx] = '\0';
	struct config_value *value = config_mappings;
	while (value->prop_name != NULL && strcmp(line, value->prop_name))
		++value;
	if (value->prop_name == NULL) {
		fprintf(stderr,
			"server.properties: invalid config property \"%s\"\n",
			line);
		return 0;
	}

	char *value_str = line + eq_idx + 1;
	int err = 0;
	switch (value->type) {
	case CV_NUM:
		if (sscanf(value_str, "%u", (uint32_t *) value->prop_field)
		    != 1)
			err = 1;
		break;
	case CV_BOOL:
		if (strcmp(value_str, "false"))
			*(bool *) (value->prop_field) = false;
		else if (strcmp(value_str, "true"))
			*(bool *) (value->prop_field) = true;
		else
			err = 1;
		break;
	case CV_STR:
		if (*value_str != '\0')
			*(char **) (value->prop_field) = strdup(value_str);
		break;
	case CV_ESTR:
		if (process_estr(value_str, value->data.default_estr,
				 value->prop_field)
		    < 0)
			return -1;
		break;
	default:
		fprintf(stderr, "unrecognized cv_type for \"%s\"\n",
			value->prop_name);
		return -1;
	}
	if (err == -1) {
		fprintf(stderr,
			"server.properties: invalid value for %s: \"%s\"\n",
			value->prop_name, value_str);
	}
	return err;
}

enum config_err read_server_properties(const char *path)
{
	FILE *properties_file = fopen(path, "r");
	if (properties_file == NULL)
		return CONFIG_READ;
	char line[LINE_LEN];
	int line_num = 1;
	enum config_err err = CONFIG_OK;
	while (err == CONFIG_OK
	       && fgets(line, LINE_LEN, properties_file) != NULL) {
		if (line[0] != '#' && line[0] != '\n') {
			int i = 0;
			while (line[i] != '\n')
				++i;
			line[i] = '\0';
			if (process_value(line_num, line) < 0)
				err = CONFIG_INVALID;
		}
		++line_num;
	}
	fclose(properties_file);
	return err;
}

int set_default_server_properties(const char *path)
{
	FILE *conf_file = fopen(path, "w");
	if (conf_file == NULL) {
		fprintf(stderr, "failed to open \"%s\": %s", path,
			strerror(errno));
		return -1;
	}

	time_t now = time(NULL);
	fprintf(conf_file, "# Minecraft server properties\n# %s", ctime(&now));

	struct config_value *value = config_mappings;
	while (value->prop_name != NULL) {
		fprintf(conf_file, "%s=", value->prop_name);
		switch (value->type) {
		case CV_NUM:
			fprintf(conf_file, "%u", value->data.default_num);
			*(uint32_t *) value->prop_field =
			    value->data.default_num;
			break;
		case CV_BOOL:
			fprintf(conf_file,
				value->data.default_bool ? "true" : "false");
			*(bool *) value->prop_field = value->data.default_bool;
			break;
		case CV_STR:
			if (value->data.default_str != NULL) {
				fprintf(conf_file, "%s",
					value->data.default_str);
				*(char **) value->prop_field =
				    strdup(value->data.default_str);
			}
			break;
		case CV_ESTR:
			fprintf(conf_file, "%s",
				value->data.default_estr->name);
			*(int *) value->prop_field =
			    value->data.default_estr->value;
			break;
		default:
			break;
		}
		fputc('\n', conf_file);
		++value;
	}
	fclose(conf_file);
	return 0;
}

void free_server_properties()
{
	struct config_value *value = config_mappings;
	while (value->prop_name != NULL) {
		if (value->type == CV_STR)
			free(*(char **) value->prop_field);
		++value;
	}
}

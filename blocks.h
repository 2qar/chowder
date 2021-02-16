#ifdef BLOCK_NAMES
extern char **block_names;
#endif

char *read_blocks_json(char *block_json_path);
int create_block_table_from_json(char *blocks_json);
int create_block_table(char *block_json_path);
int block_id(char *name);

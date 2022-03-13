#include "parser.h"

void put_id(const char *packet_name, int id);
void put_includes();
void generate_structs(const char *name, struct field *fields);
void put_function_signatures(const char *name);
void generate_source(int id, const char *name, struct field *head);

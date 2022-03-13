// hash and print each of the arguments given
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"

int main(int argc, char *argv[])
{
	if (argc == 1) {
		fprintf(stderr, "usage: ./fnv-util [STRINGS...]\n");
		return 1;
	}

	for (int i = 1; i < argc; ++i) {
		uint16_t hash = str_fnv1a(argv[i], strlen(argv[i]));
		printf("%s: 0x%04x\n", argv[i], hash);
	}
}

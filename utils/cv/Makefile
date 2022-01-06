CC=cc
CPPFLAGS=$(addprefix -I,$(addsuffix /include/,$(lib_paths)) ../../src)
CFLAGS=-g -Wall -Wextra -Werror -pedantic -DBLOCK_NAMES
LDFLAGS=-lm -lz
TARGET=cv

libs=anvil list hashmap json nbt
lib_paths=$(addprefix ../../libs/,$(libs))
vpath %.c $(lib_paths) ../../src
sources=main.c anvil.c blocks.c chunk.c section.c nbt.c list.c hashmap.c json.c
objects=$(sources:.c=.o)
valgrind_flags=--leak-check=full --show-reachable=yes

$(TARGET): $(objects)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY=leak
leak: $(TARGET)
	valgrind $(valgrind_flags) ./$(TARGET) ../../levels/default/region/r.0.0.mca 0,0

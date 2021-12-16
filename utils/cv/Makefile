CC=cc
CPPFLAGS=-I../../ -I../../include -I../../include/json/include
CFLAGS=-g -Wall -Wextra -Werror -pedantic -DBLOCK_NAMES
LDFLAGS=-lm -lz
MAIN_SOURCES=anvil.c blocks.c chunk.c section.c nbt.c include/linked_list.c include/hashmap.c include/json/json.c
SOURCES=main.c $(addprefix ../../,$(MAIN_SOURCES))
OBJFILES=$(SOURCES:.c=.o)
VALGRIND_FLAGS=--leak-check=full --show-reachable=yes
TARGET=cv

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

leak: $(TARGET)
	valgrind $(VALGRIND_FLAGS) ./$(TARGET) ../../levels/default/region/r.0.0.mca 0,0

CC=cc
CPPFLAGS=-I../../include -I../../include/json/include
CFLAGS=-g -Wall -Wextra -Werror -pedantic -DBLOCK_NAMES
LDFLAGS=-lm -lz
OBJFILES=main.o ../../region.o ../../blocks.o ../../nbt.o ../../section.o ../../include/linked_list.o ../../include/hashmap.o ../../include/json/json.o
VALGRIND_FLAGS=--leak-check=full --show-reachable=yes
TARGET=cv

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

leak: $(TARGET)
	valgrind $(VALGRIND_FLAGS) ./$(TARGET) ../../levels/default/region/r.0.0.mca 0,0

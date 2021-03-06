CC=cc
CFLAGS=-g -Wall -Wextra -Werror -pedantic -DBLOCK_NAMES
LDFLAGS=-lm -lz
OBJFILES=main.o ../../region.o ../../blocks.o ../../nbt.o ../../section.o ../../include/linked_list.o
TARGET=cv

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

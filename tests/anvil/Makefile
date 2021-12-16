CC=cc
CPPFLAGS=-I../../include/ -I../../include/json/include -I../../
CFLAGS=-g -Wall -Werror -Wextra -pedantic
LDFLAGS=-lz -lm
SOURCES=main.c ../../chunk.c ../../section.c ../../anvil.c ../../blocks.c \
	../../include/hashmap.c ../../nbt.c ../../include/json/json.c \
	../../include/linked_list.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=test

$(TARGET): $(OBJECTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

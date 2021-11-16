CC=cc
CPPFLAGS=-I../ -I../include -I../include/json/include
CFLAGS=-g -Wall -Wextra -Werror -pedantic
LIBS=-lz -lm
SOURCES=main.c read_region.c ../region.c ../nbt.c ../blocks.c ../section.c ../include/linked_list.c ../include/hashmap.c ../include/json/json.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=tests

$(TARGET): $(OBJECTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

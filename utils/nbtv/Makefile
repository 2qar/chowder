CC=cc
CPPFLAGS=$(addprefix -I,$(addsuffix /include,$(lib_paths)))
CFLAGS=-g -Wall -Wextra -Werror -pedantic
LDFLAGS=-lz
TARGET=nbtv

libs=nbt list
lib_paths=$(addprefix ../../libs/,$(libs))
vpath %.c $(lib_paths)
sources=main.c nbt.c list.c
objects=$(sources:.c=.o)

$(TARGET): $(objects)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

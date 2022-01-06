CC=gcc
CPPFLAGS=-Iinclude -I../list/include -I../hashmap/include
CFLAGS=-g -Wall -Wextra -Werror -pedantic
TARGET=json.o

sources=json.c list.c hashmap.c
vpath %.c ./:../hashmap:../list
objects=$(sources:.c=.o)

all: $(TARGET) tests

$(TARGET): $(objects)

tests: $(TARGET) tests.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o tests tests.c $(objects) -lm

.PHONY=clean
clean:
	rm $(objects) tests

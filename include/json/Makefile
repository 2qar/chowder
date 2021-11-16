CC=gcc
CPPFLAGS=-Iinclude -I../
CFLAGS=-g -Wall -Wextra -Werror -pedantic
SOURCES=json.c ../linked_list.c ../hashmap.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=json.o

all: $(TARGET)

$(TARGET): $(OBJECTS)

tests: $(TARGET) tests.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o tests tests.c $(OBJECTS) -lm

.PHONY=clean
clean:
	- rm -f $(OBJECTS) tests

CC=cc
CPPFLAGS=-I../../include/ -I../../include/json/include -I../../
CFLAGS=-g -Wall -Werror -Wextra -pedantic
SOURCES=main.c ../../blocks.c ../../include/linked_list.c ../../include/hashmap.c \
	../../include/json/json.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=test

$(TARGET): $(OBJECTS) states.txt
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJECTS) -o $@

states.txt: blocks.py
	python3 blocks.py


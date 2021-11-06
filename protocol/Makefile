CC=cc
CPPFLAGS=-Ipacket-auto-gen/include
CFLAGS=-g -Wall -Wextra -Werror -pedantic

SOURCES=$(wildcard packets/*.packet)
OBJECTS=$(SOURCES:packets/%.packet=include/%.h)
TARGET=include/protocol_autogen.h

$(TARGET): $(OBJECTS)
	./make_protocol_header.sh

include/%.h: packets/%.packet
	./packet-auto-gen/pc -o include/ $(@:include/%.h=packets/%.packet)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(@:.h=.c) -o bin/$(@:include/%.h=%.o)
	rm $(@:.h=.c)

.PHONY=clean
clean:
	@rm include/* bin/*

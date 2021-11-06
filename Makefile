CC=cc
CPPFLAGS=-Iinclude/json/include/ -Iinclude/ -Iprotocol/include -Iprotocol/packet-auto-gen/include
CFLAGS=-Wall -Wextra -Werror -pedantic
LDFLAGS=`pkg-config --libs openssl libcurl` -lm -lz
PROTOCOL_SOURCES=$(wildcard protocol/packets/*.packet)
PROTOCOL_OBJECTS=$(PROTOCOL_SOURCES:protocol/packets/%.packet=protocol/bin/%.o)
TARGET=chowder

$(TARGET): main.o protocol.o login.o conn.o packet.o player.o nbt.o region.o rsa.o section.o server.o blocks.o world.o anvil.o chunk.o include/linked_list.o include/hashmap.o include/json/json.o $(PROTOCOL_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

debug: CFLAGS += -g
debug: $(TARGET)

main.o: login.o conn.o rsa.o world.o server.o

server.o: conn.o packet.o protocol.o world.o login.o $(PROTOCOL_OBJECTS)

login.o: conn.o $(PROTOCOL_OBJECTS)

conn.o: packet.o player.o

packet.o: nbt.o

chunk.o: section.o

region.o: chunk.o

world.o: anvil.o region.o

anvil.o: region.o nbt.o

$(PROTOCOL_OBJECTS):
	@cd protocol/ && make && cd ../

clean:
	rm -f *.o include/*.o $(TARGET)

CC=cc
CPPFLAGS=-Iinclude/json/include/ -Iinclude/
CFLAGS=-Wall -Wextra -Werror -pedantic
LDFLAGS=`pkg-config --libs openssl libcurl` -lm -lz
TARGET=chowder

$(TARGET): main.o protocol.o login.o conn.o packet.o player.o nbt.o region.o rsa.o section.o server.o blocks.o world.o anvil.o chunk.o include/linked_list.o include/hashmap.o include/json/json.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

debug: CFLAGS += -g
debug: $(TARGET)

main.o: protocol.o login.o conn.o rsa.o world.o server.o

server.o: conn.o packet.o world.o login.o protocol.o

protocol.o: nbt.o packet.o conn.o region.o

login.o: protocol.o conn.o

conn.o: packet.o player.o

packet.o: nbt.o

chunk.o: section.o

region.o: chunk.o

world.o: anvil.o region.o

anvil.o: region.o nbt.o

clean:
	rm -f *.o include/*.o $(TARGET)

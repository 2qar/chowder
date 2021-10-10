CC=cc
CFLAGS=-Wall -Wextra -Werror -pedantic
LIBSSL=`pkg-config --libs openssl`
LIBS=$(LIBSSL) -lm -lz
TARGET=chowder

$(TARGET): main.o protocol.o login.o conn.o packet.o player.o nbt.o region.o rsa.o section.o server.o blocks.o world.o include/linked_list.o include/hashmap.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

debug: CFLAGS += -g
debug: $(TARGET)

main.o: protocol.o login.o conn.o rsa.o world.o server.o

server.o: conn.o packet.o world.o login.o protocol.o

protocol.o: nbt.o packet.o conn.o region.o

login.o: protocol.o conn.o

conn.o: packet.o player.o

packet.o: nbt.o

region.o: section.o nbt.o

world.o: region.o

clean:
	rm -f *.o include/*.o $(TARGET)

CC=cc
CFLAGS=-Wall -Wextra -Werror -pedantic
LIBSSL=`pkg-config --libs openssl`
LIBS=$(LIBSSL) -lm -lz
TARGET=chowder

$(TARGET): main.o protocol.o server.o conn.o packet.o nbt.o region.o blockstates.o blocks.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

main.o: protocol.o server.o conn.o

protocol.o: nbt.o packet.o conn.o

server.o: protocol.o conn.o

conn.o: packet.o

packet.o: nbt.o

region.o: blockstates.o nbt.o

clean:
	rm -f *.o $(TARGET)

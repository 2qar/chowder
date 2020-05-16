make:
	cc main.c packet.c protocol.c server.c -Wall -lssl -lcrypto -o chowder

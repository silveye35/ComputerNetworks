all: prog2_server prog2_client
CC=gcc
CFLAGS=-g -Wall

prog2_server: prog2_server.c trie.c
	$(CC) -o prog2_server prog2_server.c trie.c $(CFLAGS)

prog2_client: prog2_client.c
	$(CC) -o prog2_client prog2_client.c $(CFLAGS)

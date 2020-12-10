# make rule primaria
all: client server

# make rule per il client
client: client.o
	gcc -Wall client.o -o client

# make rule per il server
server: server.o
	gcc -Wall server.o -o server

# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o client server
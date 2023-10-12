all: client.c server.c
	gcc client.c -o client
	gcc server.c -o server
	gcc dclient.c -o dclient
	gcc dserver.c -o dserver
clean:
	rm client server dclient dserver *recv*
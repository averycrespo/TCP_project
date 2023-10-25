all: server client client 

server: server.cpp
	g++ -g -Wall server.cpp -o server -lpthread

# if you wish to add anopther client
# g++ -g -Wall client_directoryX/client.cpp -o client_directoryX/client
# replace X with the directory
# example: g++ -g -Wall client_directory3/client.cpp -o client_directory3/client
client: client_directory1/client.cpp
	g++ -g -Wall client_directory1/client.cpp -o client_directory1/client
	g++ -g -Wall client_directory2/client.cpp -o client_directory2/client


clean:
	rm -f server client_directory*/client
	
all: client server
client:client.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17 -pthread
server:server.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17 -pthread
	
.PHONY:
clean:
	rm -rf server client
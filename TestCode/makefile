all: client server
client:test_client.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17 -pthread
server:test_server.cc
	g++ -std=c++11 -o $@ $^ -g -std=c++17 -pthread
	
.PHONY:
clean:
	rm -rf server client
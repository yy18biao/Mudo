all: client server
client:test_client.cc
	g++ -std=c++11 -o $@ $^ -g
server:test_server.cc
	g++ -std=c++11 -o $@ $^ -g
	
.PHONY:
clean:
	rm -rf server client
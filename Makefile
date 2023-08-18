all: server subscriber

# Compileaza server.c
server: server.cpp utils.cpp
		g++ -g -Wall server.cpp utils.cpp -o server

# Compileaza subscriber.c
subscriber: subscriber.cpp utils.cpp
		g++ -g -Wall subscriber.cpp utils.cpp -o subscriber

clean:
	rm -rf server subscriber *.o 

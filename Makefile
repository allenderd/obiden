all:	$(DIR)obiden $(DIR)raft $(DIR)client

obiden: main.cpp host.cpp networking.cpp timer.cpp timer.h packets.h networking.h host.h
	g++ -std=c++0x -Wall -o obiden main.cpp host.cpp networking.cpp timer.cpp -lpthread

raft: main.cpp host.cpp networking.cpp timer.cpp timer.h packets.h networking.h host.h
	g++ -DRAFT_MODE -std=c++0x -Wall -o raft main.cpp host.cpp networking.cpp timer.cpp -lpthread

client: client.cpp packets.h networking.h host.h
	g++ -std=c++0x -Wall -o client client.cpp -lpthread

clean:
	rm -f *.o *~ obiden client raft

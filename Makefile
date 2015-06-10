all:	$(DIR)obiden $(DIR)raft $(DIR)client

obiden: main.cpp host.cpp networking.cpp timer.cpp timer.h packets.h networking.h host.h debug_print.h
	g++ -g -DVERBOSE_TIMING -std=c++0x -Wall -o obiden main.cpp host.cpp networking.cpp timer.cpp -lpthread

raft: main.cpp host.cpp networking.cpp timer.cpp timer.h packets.h networking.h host.h debug_print.h
	g++ -DVERBOSE_TIMING -DRAFT_MODE -std=c++0x -Wall -o raft main.cpp host.cpp networking.cpp timer.cpp -lpthread

client: client.cpp packets.h networking.h host.h
	g++ -std=c++0x -Wall -o client client.cpp -lpthread
	
president_demo: president_demo.cpp packets.h
	g++ -g -std=c++0x -Wall -o president_demo president_demo.cpp -lpthread

follower_demo: follower_demo.cpp packets.h
	g++ -g -std=c++0x -Wall -o follower_demo follower_demo.cpp -lpthread

clean:
	rm -f *.o *~ obiden client raft president_demo follower_demo

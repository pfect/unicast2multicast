DEBUGFLAGS = -g -W -Wall
BUILDFLAGS = $(DEBUGFLAGS)
CC = gcc

all: unicast2multicast multicast2unicast

unicast2multicast: unicast2multicast.o
	$(CC) -g -o unicast2multicast unicast2multicast.o
	
multicast2unicast: multicast2unicast.o
	$(CC) -g -o multicast2unicast multicast2unicast.o

unicast2multicast.o: unicast2multicast.c
	$(CC) $(BUILDFLAGS) -c -o unicast2multicast.o unicast2multicast.c

multicast2unicast.o: multicast2unicast.c
	$(CC) $(BUILDFLAGS) -c -o multicast2unicast.o multicast2unicast.c

clean:
	rm -f *.o
	rm -f unicast2multicast

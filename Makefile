CFLAGS=-g -O0 -Wall -W -pipe $(shell pkg-config --cflags glib-2.0) -Wno-unused
LIBS=$(shell pkg-config --libs glib-2.0)

all: flexmdns prioq-test

flexmdns: main.o iface.o netlink.o server.o address.o util.o prioq.o
	$(CC) -o $@ $^ $(LIBS)

#test-llist: test-llist.o
#	$(CC) -o $@ $^ $(LIBS)

prioq-test: prioq-test.o prioq.o
	$(CC) -o $@ $^ $(LIBS)


*.o: *.h

clean:
	rm -f *.o flexmdns

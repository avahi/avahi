#CC=gcc-2.95
CFLAGS=-g -O1 -Wall -W -pipe $(shell pkg-config --cflags glib-2.0) -Wno-unused
LIBS=$(shell pkg-config --libs glib-2.0)

all: strlst-test prioq-test domain-test dns-test flexmdns

flexmdns: timeeventq.o main.o iface.o netlink.o server.o address.o util.o prioq.o cache.o rr.o dns.o socket.o psched.o announce.o subscribe.o strlst.o
	$(CC) -o $@ $^ $(LIBS)

#test-llist: test-llist.o
#	$(CC) -o $@ $^ $(LIBS)

prioq-test: prioq-test.o prioq.o
	$(CC) -o $@ $^ $(LIBS)

strlst-test: strlst-test.o strlst.o
	$(CC) -o $@ $^ $(LIBS)

domain-test: domain-test.o util.o
	$(CC) -o $@ $^ $(LIBS)

dns-test: dns-test.o util.o dns.o rr.o strlst.o
	$(CC) -o $@ $^ $(LIBS)

*.o: *.h

clean:
	rm -f *.o flexmdns prioq-test strlst-test domain-test dns-test

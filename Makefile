CFLAGS=-g -O0 -Wall -W -pipe $(shell pkg-config --cflags glib-2.0)
LIBS=$(shell pkg-config --libs glib-2.0)

flexmdns: main.o iface.o netlink.o server.o address.o util.o local.o
	$(CC) -o $@ $^ $(LIBS)

*.o: *.h

clean:
	rm -f *.o flexmdns

CFLAGS=-Wall -fPIC -g -O0 -W -pipe '-DDEBUG_TRAP=__asm__("int $$3")'

all: query nsstest libnss_mdns.so.2 libnss_mdns6.so.2 libnss_mdns4.so.2 

query: query.o dns.o util.o main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

nsstest: nsstest.o

libnss_mdns.so.2: query.o dns.o util.o nss.o
	$(CC) -shared -o $@ -Wl,-soname,$@ $^

libnss_mdns4.so.2: query.o dns.o util.o nss4.o
	$(CC) -shared -o $@ -Wl,-soname,$@ $^

libnss_mdns6.so.2: query.o dns.o util.o nss6.o
	$(CC) -shared -o $@ -Wl,-soname,$@ $^

nss6.o: nss.c
	$(CC) $(CFLAGS) -DNSS_IPV6_ONLY=1 -c -o $@ $<

nss4.o: nss.c
	$(CC) $(CFLAGS) -DNSS_IPV4_ONLY=1 -c -o $@ $<

*.o: *.h

clean:
	rm -f *.o query *.so.2 nsstest

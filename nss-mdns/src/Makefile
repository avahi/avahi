CFLAGS=-Wall -fPIC -g -O0 -W -pipe '-DDEBUG_TRAP=__asm__("int $$3")'

all: mdns-test nss-test libnss_mdns.so.2 libnss_mdns6.so.2 libnss_mdns4.so.2 

mdns-test: query.o dns.o util.o mdns-test.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

nss-test: nss-test.o

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
	rm -f *.o mdns-test *.so.2 nss-test

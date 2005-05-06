#include "dns.h"
#include "util.h"

int main(int argc, char *argv[]) {
    gchar t[256], *a, *b, *c, *d;
    AvahiDnsPacket *p;

    p = avahi_dns_packet_new(8000);

    avahi_dns_packet_append_name(p, a = "hello.hello.hello.de.");
    avahi_dns_packet_append_name(p, b = "this is a test.hello.de."); 
    avahi_dns_packet_append_name(p, c = "this\\.is\\.a\\.test\\.with\\.dots.hello.de."); 
    avahi_dns_packet_append_name(p, d = "this\\\\is another\\ \\test.hello.de."); 

    avahi_hexdump(AVAHI_DNS_PACKET_DATA(p), p->size);

    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(a, t));
    
    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(b, t));

    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(c, t));

    avahi_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(avahi_domain_equal(d, t));
    
    avahi_dns_packet_free(p);
    return 0;
}

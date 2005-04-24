#include "dns.h"
#include "util.h"

int main(int argc, char *argv[]) {
    gchar t[256], *a, *b, *c, *d;
    flxDnsPacket *p;

    p = flx_dns_packet_new(8000);

    flx_dns_packet_append_name(p, a = "hello.hello.hello.de.");
    flx_dns_packet_append_name(p, b = "this is a test.hello.de."); 
    flx_dns_packet_append_name(p, c = "this\\.is\\.a\\.test\\.with\\.dots.hello.de."); 
    flx_dns_packet_append_name(p, d = "this\\\\is another\\ \\test.hello.de."); 

    flx_hexdump(FLX_DNS_PACKET_DATA(p), p->size);

    flx_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(flx_domain_equal(a, t));
    
    flx_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(flx_domain_equal(b, t));

    flx_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(flx_domain_equal(c, t));

    flx_dns_packet_consume_name(p, t, sizeof(t));
    g_message(">%s<", t);
    g_assert(flx_domain_equal(d, t));
    
    flx_dns_packet_free(p);
    return 0;
}

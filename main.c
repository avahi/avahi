#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "flx.h"

static GMainLoop *loop = NULL;

static gboolean timeout(gpointer data) {
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char *argv[]) {
    flxServer *flx;
    flxLocalAddrSource *l;
    flxAddress a;
    gchar *r;

    flx = flx_server_new(NULL);

    l = flx_local_addr_source_new(flx);

    flx_address_parse("127.0.0.1", AF_INET, &a);
    flx_server_add_address(flx, 0, 0, "localhost", &a);

    flx_address_parse("::1", AF_INET6, &a);
    flx_server_add_address(flx, 0, 0, "ip6-localhost", &a);

    g_timeout_add(1000, timeout, NULL);
    
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    flx_server_dump(flx, stdout);

    flx_local_addr_source_free(l);
    flx_server_free(flx);
    return 0;
}

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "flx.h"

static GMainLoop *loop = NULL;

static gboolean timeout (gpointer data) {
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char *argv[]) {
    flxServer *flx;
    flxLocalAddrSource *l;
    guint32 ip;

    flx = flx_server_new(NULL);

    l = flx_local_addr_source_new(flx);
    
/*     ip = inet_addr("127.0.0.1"); */
/*     flx_server_add(flx, flx_server_get_next_id(flx), "foo.local", FLX_DNS_TYPE_A, &ip, sizeof(ip)); */

    g_timeout_add(1000, timeout, NULL);
    
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    flx_server_dump(flx, stdout);

    flx_local_addr_source_free(l);
    flx_server_free(flx);
    return 0;
}

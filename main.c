#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "flx.h"
#include "server.h"

static gboolean timeout(gpointer data) {
    g_main_loop_quit(data);
    return FALSE;
}

int main(int argc, char *argv[]) {
    flxServer *flx;
    gchar *r;
    flxKey *k;
    GMainLoop *loop = NULL;

    flx = flx_server_new(NULL);

    flx_server_add_text(flx, 0, 0, AF_UNSPEC, FALSE, NULL, "hallo");

    k = flx_key_new("cocaine.local.", FLX_DNS_CLASS_IN, FLX_DNS_TYPE_A);
    flx_server_send_query(flx, 0, AF_UNSPEC, k);
    flx_key_unref(k);

    loop = g_main_loop_new(NULL, FALSE);
    
    g_timeout_add(5000, timeout, loop);
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    flx_server_dump(flx, stdout);

    flx_server_free(flx);
    
    return 0;
}

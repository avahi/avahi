#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "flx.h"
#include "server.h"

static gboolean quit_timeout(gpointer data) {
    g_main_loop_quit(data);
    return FALSE;
}

static gboolean send_timeout(gpointer data) {
    flxServer *flx = data;
    flxKey *k;

    /*     k = flx_key_new("cocaine.local.", FLX_DNS_CLASS_IN, FLX_DNS_TYPE_A); */
/*     flx_server_post_query(flx, 0, AF_UNSPEC, k); */
/*     flx_key_unref(k); */

    k = flx_key_new("ecstasy.local.", FLX_DNS_CLASS_IN, FLX_DNS_TYPE_A);
    flx_server_post_query(flx, 0, AF_INET, k);
    flx_key_unref(k);

    return FALSE;
}

static gboolean dump_timeout(gpointer data) {
    flxServer *flx = data;
    flx_server_dump(flx, stdout);
    return TRUE;
}

int main(int argc, char *argv[]) {
    flxServer *flx;
    gchar *r;
    GMainLoop *loop = NULL;

    flx = flx_server_new(NULL);

    flx_server_add_text(flx, 0, 0, AF_UNSPEC, FALSE, NULL, "hallo");

    loop = g_main_loop_new(NULL, FALSE);
    
    g_timeout_add(1000*60, quit_timeout, loop);
    g_timeout_add(1000, send_timeout, flx);
    g_timeout_add(1000*10, dump_timeout, flx);
    
    g_main_loop_run(loop);

    g_main_loop_unref(loop);


    flx_server_free(flx);
    
    return 0;
}

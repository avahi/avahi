#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "flx.h"
#include "server.h"

static GMainLoop *loop = NULL;

static gboolean timeout(gpointer data) {
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char *argv[]) {
    flxServer *flx;
    gchar *r;
    flxQuery q;

    flx = flx_server_new(NULL);

    flx_server_add_text(flx, 0, 0, AF_UNSPEC, NULL, "hallo");
    
    q.name = "campari.local.";
    q.class = FLX_DNS_CLASS_IN;
    q.type = FLX_DNS_TYPE_A;
    flx_server_post_query_job(flx, 0, AF_UNSPEC, NULL, &q);
    
    g_timeout_add(5000, timeout, NULL);
    
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    flx_server_dump(flx, stdout);

    flx_server_free(flx);
    return 0;
}

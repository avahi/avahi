#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "flx.h"
#include "server.h"
#include "subscribe.h"

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

/*     k = flx_key_new("ecstasy.local.", FLX_DNS_CLASS_IN, FLX_DNS_TYPE_A); */
/*     flx_server_post_query(flx, 0, AF_INET, k); */
/*     flx_key_unref(k); */

    return FALSE;
}

static gboolean dump_timeout(gpointer data) {
    flxServer *flx = data;
    flx_server_dump(flx, stdout);
    return TRUE;
}

static void subscription(flxSubscription *s, flxRecord *r, gint interface, guchar protocol, flxSubscriptionEvent event, gpointer userdata) {
    gchar *t;
    
    g_assert(s);
    g_assert(r);
    g_assert(interface > 0);
    g_assert(protocol != AF_UNSPEC);

    g_message("SUBSCRIPTION: record [%s] on %i.%i is %s", t = flx_record_to_string(r), interface, protocol,
              event == FLX_SUBSCRIPTION_NEW ? "new" : (event == FLX_SUBSCRIPTION_CHANGE ? "changed" : "removed"));

    g_free(t);
}


int main(int argc, char *argv[]) {
    flxServer *flx;
    gchar *r;
    GMainLoop *loop = NULL;
    flxSubscription *s;
    flxKey *k;

    flx = flx_server_new(NULL);

    flx_server_add_text(flx, 0, 0, AF_UNSPEC, FALSE, NULL, "hallo");

/*     k = flx_key_new("_http._tcp.local.", FLX_DNS_CLASS_IN, FLX_DNS_TYPE_PTR); */
/*     s = flx_subscription_new(flx, k, 0, AF_UNSPEC, subscription, NULL); */
/*     flx_key_unref(k); */

    loop = g_main_loop_new(NULL, FALSE);
    
    g_timeout_add(1000*30, quit_timeout, loop);
    g_timeout_add(1000, send_timeout, flx);
    g_timeout_add(1000*20, dump_timeout, flx);
    
    g_main_loop_run(loop);

    g_main_loop_unref(loop);

/*     flx_subscription_free(s); */
    flx_server_free(flx);
    
    return 0;
}

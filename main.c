#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "flx.h"

static gboolean quit_timeout(gpointer data) {
    g_main_loop_quit(data);
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

static void entry_group_callback(flxServer *s, flxEntryGroup *g, flxEntryGroupState state, gpointer userdata) {
    g_message("entry group state: %i", state);
}

int main(int argc, char *argv[]) {
    flxServer *flx;
    gchar *r;
    GMainLoop *loop = NULL;
    flxSubscription *s;
    flxKey *k;
    flxEntryGroup *g;

    flx = flx_server_new(NULL);

    g = flx_entry_group_new(flx, entry_group_callback, NULL); 
    
/*    flx_server_add_text(flx, g, 0, AF_UNSPEC, FLX_ENTRY_UNIQUE, NULL, "hallo", NULL); */
     flx_server_add_service(flx, g, 0, AF_UNSPEC, "_http._tcp", "gurke", NULL, NULL, 80, "foo", NULL); 
    
    flx_entry_group_commit(g); 

    flx_server_dump(flx, stdout);
    
    
/*     k = flx_key_new("ecstasy.local.", FLX_DNS_CLASS_IN, FLX_DNS_TYPE_ANY); */
/*     s = flx_subscription_new(flx, k, 0, AF_UNSPEC, subscription, NULL); */
/*     flx_key_unref(k); */

    loop = g_main_loop_new(NULL, FALSE);
    
    g_timeout_add(1000*20, dump_timeout, flx);
    g_timeout_add(1000*30, quit_timeout, loop);
    
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

/*     flx_subscription_free(s); */
    flx_entry_group_free(g); 
    flx_server_free(flx);
    
    return 0;
}

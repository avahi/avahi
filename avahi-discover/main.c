#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <avahi-core/core.h>
#include <avahi-common/strlst.h>

struct ServiceType;

struct Service {
    struct ServiceType *service_type;
    gchar *service_name;
    gchar *domain_name;

    gint interface;
    guchar protocol;

    GtkTreeRowReference *tree_ref;
};

struct ServiceType {
    gchar *service_type;
    AvahiServiceBrowser *browser;

    GList *services;
    GtkTreeRowReference *tree_ref;
};

static GtkWidget *main_window = NULL;
static GtkTreeView *tree_view = NULL;
static GtkTreeStore *tree_store = NULL;
static GtkLabel *info_label = NULL;
static AvahiServer *server = NULL;
static AvahiServiceTypeBrowser *service_type_browser = NULL;
static GHashTable *service_type_hash_table = NULL;
static AvahiServiceResolver *service_resolver = NULL;
static struct Service *current_service = NULL;

/* very, very ugly: just import these two internal but useful functions from libavahi-core by hand */
guint avahi_domain_hash(const gchar *s);
gboolean avahi_domain_equal(const gchar *a, const gchar *b);

static struct Service *get_service(const gchar *service_type, const gchar *service_name, const gchar*domain_name, gint interface, guchar protocol) {
    struct ServiceType *st;
    GList *l;

    if (!(st = g_hash_table_lookup(service_type_hash_table, service_type)))
        return NULL;

    for (l = st->services; l; l = l->next) {
        struct Service *s = l->data;
        
        if (s->interface == interface &&
            s->protocol == protocol &&
            avahi_domain_equal(s->service_name, service_name) &&
            avahi_domain_equal(s->domain_name, domain_name))
            return s;
    }

    return NULL;
}

static void free_service(struct Service *s) {
    GtkTreeIter iter;
    GtkTreePath *path;

    if (current_service == s) {
        current_service = NULL;

        if (service_resolver) {
            avahi_service_resolver_free(service_resolver);
            service_resolver = NULL;
        }
 
        gtk_label_set_text(info_label, "<i>Service removed</i>");
    }
    
    s->service_type->services = g_list_remove(s->service_type->services, s);

    path = gtk_tree_row_reference_get_path(s->tree_ref);
    gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, path);
    gtk_tree_path_free(path);
    
    gtk_tree_store_remove(tree_store, &iter);

    gtk_tree_row_reference_free(s->tree_ref);

    g_free(s->service_name);
    g_free(s->domain_name);
    g_free(s);
}

static void service_browser_callback(AvahiServiceBrowser *b, gint interface, guchar protocol, AvahiBrowserEvent event, const gchar *service_name, const gchar *service_type, const gchar *domain_name, gpointer userdata) {

    if (event == AVAHI_BROWSER_NEW) {
        struct Service *s;
        GtkTreeIter iter, piter;
        GtkTreePath *path, *ppath;
        gchar iface[256];
        
        s = g_new(struct Service, 1);
        s->service_name = g_strdup(service_name);
        s->domain_name = g_strdup(domain_name);
        s->interface = interface;
        s->protocol = protocol;
        s->service_type = g_hash_table_lookup(service_type_hash_table, service_type);
        g_assert(s->service_type);
        
        s->service_type->services = g_list_prepend(s->service_type->services, s);

        ppath = gtk_tree_row_reference_get_path(s->service_type->tree_ref);
        gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &piter, ppath);

        snprintf(iface, sizeof(iface), "#%i %s", interface, protocol == AF_INET ? "IPv4" : "IPv6");

        gtk_tree_store_append(tree_store, &iter, &piter);
        gtk_tree_store_set(tree_store, &iter, 0, s->service_name, 1, iface, 2, s, -1);

        path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), &iter);
        s->tree_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(tree_store), path);
        gtk_tree_path_free(path);

        gtk_tree_view_expand_row(tree_view, ppath, FALSE);
        gtk_tree_path_free(ppath);

    
    } else if (event == AVAHI_BROWSER_REMOVE) {
        struct Service* s;

        if ((s = get_service(service_type, service_name, domain_name, interface, protocol)))
            free_service(s);
    }
}

static void service_type_browser_callback(AvahiServiceTypeBrowser *b, gint interface, guchar protocol, AvahiBrowserEvent event, const gchar *service_type, const gchar *domain, gpointer userdata) {
    struct ServiceType *st;
    GtkTreePath *path;
    GtkTreeIter iter;

    if (event != AVAHI_BROWSER_NEW)
        return;

    if (g_hash_table_lookup(service_type_hash_table, service_type))
        return;

    st = g_new(struct ServiceType, 1);
    st->service_type = g_strdup(service_type);
    st->services = NULL;
    
    gtk_tree_store_append(tree_store, &iter, NULL);
    gtk_tree_store_set(tree_store, &iter, 0, st->service_type, 1, "", 2, NULL, -1);

    path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), &iter);
    st->tree_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(tree_store), path);
    gtk_tree_path_free(path);

    g_hash_table_insert(service_type_hash_table, st->service_type, st);

    st->browser = avahi_service_browser_new(server, -1, AF_UNSPEC, st->service_type, domain, service_browser_callback, NULL);
}

static void update_label(struct Service *s, const gchar *hostname, const AvahiAddress *a, guint16 port, AvahiStringList *txt) {
    gchar t[512], address[64], *txt_s;

    if (a && hostname) {
        char na[256];
        avahi_address_snprint(na, sizeof(na), a);
        snprintf(address, sizeof(address), "%s/%s:%u", hostname, na, port);
        txt_s = avahi_string_list_to_string(txt);
    } else {
        snprintf(address, sizeof(address), "<i>n/a</i>");
        txt_s = g_strdup("<i>n/a</i>");
    }
    
    snprintf(t, sizeof(t),
             "<b>Service Type:</b> %s\n"
             "<b>Service Name:</b> %s\n"
             "<b>Domain Name:</b> %s\n"
             "<b>Interface:</b> %i %s\n"
             "<b>Address:</b> %s\n"
             "<b>TXT Data:</b> %s",
             s->service_type->service_type,
             s->service_name,
             s->domain_name,
             s->interface,
             s->protocol == AF_INET ? "IPv4" : "IPv6",
             address,
             txt_s);

    gtk_label_set_markup(info_label, t);

    g_free(txt_s);
}

static struct Service *get_service_on_cursor(void) {
    GtkTreePath *path;
    struct Service *s;
    GtkTreeIter iter;
    
    gtk_tree_view_get_cursor(tree_view, &path, NULL);
    gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter, 2, &s, -1);
    gtk_tree_path_free(path);

    return s;
}


static void service_resolver_callback(AvahiServiceResolver *r, gint interface, guchar protocol, AvahiResolverEvent event, const gchar *name, const gchar *type, const gchar *domain, const gchar *host_name, const AvahiAddress *a, guint16 port, AvahiStringList *txt, gpointer userdata) {
    struct Service *s;
    g_assert(r);

    if (!(s = get_service_on_cursor()) || userdata != s) {
        g_assert(r == service_resolver);
        avahi_service_resolver_free(service_resolver);
        service_resolver = NULL;
    }

    if (event == AVAHI_RESOLVER_TIMEOUT)
        gtk_label_set_markup(info_label, "<i>Failed to resolve.</i>");
    else 
        update_label(s, host_name, a, port, txt);
}

static void tree_view_on_cursor_changed(GtkTreeView *tv, gpointer userdata) {
    struct Service *s;
    
    if (!(s = get_service_on_cursor()))
        return;

    if (service_resolver)
        avahi_service_resolver_free(service_resolver);

    update_label(s, NULL, NULL, 0, NULL);

    service_resolver = avahi_service_resolver_new(server, s->interface, s->protocol, s->service_name, s->service_type->service_type, s->domain_name, AF_UNSPEC, service_resolver_callback, s);
}

gboolean main_window_on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[]) {
    GladeXML *xml;
    AvahiServerConfig config;
    GtkTreeViewColumn *c;

    gtk_init(&argc, &argv);
    glade_init();

    xml = glade_xml_new("avahi-discover.glade", NULL, NULL);
    main_window = glade_xml_get_widget(xml, "main_window");
    g_signal_connect(main_window, "delete-event", (GCallback) main_window_on_delete_event, NULL);
    
    tree_view = GTK_TREE_VIEW(glade_xml_get_widget(xml, "tree_view"));
    g_signal_connect(GTK_WIDGET(tree_view), "cursor-changed", (GCallback) tree_view_on_cursor_changed, NULL);

    info_label = GTK_LABEL(glade_xml_get_widget(xml, "info_label"));

    tree_store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL(tree_store));
    gtk_tree_view_insert_column_with_attributes(tree_view, -1, "Name", gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(tree_view, -1, "Interface", gtk_cell_renderer_text_new(), "text", 1, NULL);

    gtk_tree_view_column_set_resizable(c = gtk_tree_view_get_column(tree_view, 0), TRUE);
    gtk_tree_view_column_set_sizing(c, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_expand(c, TRUE);
    
    service_type_hash_table = g_hash_table_new((GHashFunc) avahi_domain_hash, (GEqualFunc) avahi_domain_equal);
    
    avahi_server_config_init(&config);
    config.register_hinfo = config.register_addresses = config.announce_domain = config.register_workstation = FALSE;
    server = avahi_server_new(NULL, &config, NULL, NULL);
    avahi_server_config_free(&config);

    service_type_browser = avahi_service_type_browser_new(server, -1, AF_UNSPEC, argc >= 2 ? argv[1] : NULL, service_type_browser_callback, NULL);

    gtk_main();

    avahi_server_free(server);

    return 0;
}

/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdarg.h>
#include <net/if.h>

#include <gtk/gtk.h>

#include <avahi-glib/glib-watch.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/address.h>
#include <avahi-common/domain.h>

#include "avahi-ui.h"

#if defined(HAVE_GDBM) || defined(HAVE_DBM)
#include "../avahi-utils/stdb.h"
#endif

/* todo: i18n, HIGify */

struct _AuiServiceDialog {
    GtkDialog parent_instance;

    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    AvahiServiceBrowser **browsers;
    AvahiServiceResolver *resolver;
    AvahiDomainBrowser *domain_browser;
    
    gchar **browse_service_types;
    gchar *service_type;
    gchar *domain;
    gchar *service_name;
    AvahiProtocol address_family;
        
    AvahiAddress address;
    gchar *host_name;
    AvahiStringList *txt_data;
    guint16 port;

    gboolean resolve_service, resolve_service_done;
    gboolean resolve_host_name, resolve_host_name_done;
    
    GtkWidget *domain_label;
    GtkWidget *domain_button;
    GtkWidget *service_tree_view;
    GtkWidget *service_progress_bar;
    GtkWidget *service_ok_button;

    GtkListStore *service_list_store, *domain_list_store;

    guint service_pulse_timeout;
    guint domain_pulse_timeout;
    guint start_idle;

    AvahiIfIndex common_interface;
    AvahiProtocol common_protocol;

    GtkWidget *domain_dialog;
    GtkWidget *domain_entry;
    GtkWidget *domain_tree_view;
    GtkWidget *domain_progress_bar;
    GtkWidget *domain_ok_button;
};

enum {
    PROP_0,
    PROP_BROWSE_SERVICE_TYPES,
    PROP_DOMAIN,
    PROP_SERVICE_TYPE,
    PROP_SERVICE_NAME,
    PROP_ADDRESS,
    PROP_PORT,
    PROP_HOST_NAME,
    PROP_TXT_DATA,
    PROP_RESOLVE_SERVICE,
    PROP_RESOLVE_HOST_NAME,
    PROP_ADDRESS_FAMILY
};

enum {
    SERVICE_COLUMN_IFACE,
    SERVICE_COLUMN_PROTO,
    SERVICE_COLUMN_TYPE,
    SERVICE_COLUMN_NAME,
    SERVICE_COLUMN_PRETTY_IFACE,
    SERVICE_COLUMN_PRETTY_TYPE,
    N_SERVICE_COLUMNS
};

enum {
    DOMAIN_COLUMN_NAME,
    DOMAIN_COLUMN_REF,
    N_DOMAIN_COLUMNS
};

static void aui_service_dialog_finalize(GObject *object);
static void aui_service_dialog_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void aui_service_dialog_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE(AuiServiceDialog, aui_service_dialog, GTK_TYPE_DIALOG)

static void aui_service_dialog_class_init(AuiServiceDialogClass *klass) {
    GObjectClass *object_class;
    
    object_class = (GObjectClass*) klass;

    object_class->finalize = aui_service_dialog_finalize;
    object_class->set_property = aui_service_dialog_set_property;
    object_class->get_property = aui_service_dialog_get_property;

    g_object_class_install_property(
            object_class,
            PROP_BROWSE_SERVICE_TYPES,
            g_param_spec_pointer("browse_service_types", "Browse Service Types", "A NULL terminated list of service types to browse for",
                                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
            object_class,
            PROP_DOMAIN,
            g_param_spec_string("domain", "Domain", "The domain to browse in, or NULL for the default domain",
                                NULL,
                                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
            object_class,
            PROP_SERVICE_TYPE,
            g_param_spec_string("service_type", "Service Type", "The service type of the selected service",
                                NULL,
                                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
            object_class,
            PROP_SERVICE_NAME,
            g_param_spec_string("service_name", "Service Name", "The service name of the selected service",
                                NULL,
                                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
            object_class,
            PROP_ADDRESS,
            g_param_spec_pointer("address", "Address", "The address of the resolved service",
                                G_PARAM_READABLE));    
    g_object_class_install_property(
            object_class,
            PROP_PORT,
            g_param_spec_uint("port", "Port", "The IP port number of the resolved service",
                             0, 0xFFFF, 0,
                             G_PARAM_READABLE));
    g_object_class_install_property(
            object_class,
            PROP_HOST_NAME,
            g_param_spec_string("host_name", "Host Name", "The host name of the resolved service",
                                NULL,
                                G_PARAM_READABLE));
    g_object_class_install_property(
            object_class,
            PROP_TXT_DATA,
            g_param_spec_pointer("txt_data", "TXT Data", "The TXT data of the resolved service",
                                G_PARAM_READABLE));
    g_object_class_install_property(
            object_class,
            PROP_RESOLVE_SERVICE,
            g_param_spec_boolean("resolve_service", "Resolve service", "Resolve service",
                                 TRUE,
                                 G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
            object_class,
            PROP_RESOLVE_HOST_NAME,
            g_param_spec_boolean("resolve_host_name", "Resolve service host name", "Resolve service host name",
                                 TRUE,
                                 G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
            object_class,
            PROP_ADDRESS_FAMILY,
            g_param_spec_int("address_family", "Address family", "The address family for host name resolution",
                             AVAHI_PROTO_UNSPEC, AVAHI_PROTO_INET6, AVAHI_PROTO_UNSPEC,
                             G_PARAM_READABLE | G_PARAM_WRITABLE));
}


GtkWidget *aui_service_dialog_new(const gchar *title) {
    return GTK_WIDGET(g_object_new(
                              AUI_TYPE_SERVICE_DIALOG,
                              "has-separator", FALSE,
                              "title", title,
                              NULL));
}

static gboolean service_pulse_callback(gpointer data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(data);

    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(d->service_progress_bar));
    return TRUE;
}

static gboolean domain_pulse_callback(gpointer data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(data);

    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(d->domain_progress_bar));
    return TRUE;
}

static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);

    if (state == AVAHI_CLIENT_FAILURE) {
        GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE,
                                              "Avahi client failure: %s",
                                              avahi_strerror(avahi_client_errno(c)));
        gtk_dialog_run(GTK_DIALOG(m));
        gtk_widget_destroy(m);
        
        gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
    }
}

static void resolve_callback(
        AvahiServiceResolver *r,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiResolverEvent event,
        const char *name,
        const char *type,
        const char *domain,
        const char *host_name,
        const AvahiAddress *a,
        uint16_t port,
        AvahiStringList *txt,
        AvahiLookupResultFlags flags,
        void *userdata) {

    AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);

    switch (event) {
        case AVAHI_RESOLVER_FOUND:

            d->resolve_service_done = 1;

            g_free(d->service_name);
            d->service_name = g_strdup(name);

            g_free(d->service_type);
            d->service_type = g_strdup(type);

            g_free(d->domain);
            d->domain = g_strdup(domain);

            g_free(d->host_name);
            d->host_name = g_strdup(host_name);

            d->port = port;

            avahi_string_list_free(d->txt_data);
            d->txt_data = avahi_string_list_copy(txt);
            
            if (a) {
                d->resolve_host_name_done = 1;
                d->address = *a;
            }

            gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_OK);

            break;

        case AVAHI_RESOLVER_FAILURE: {
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_CLOSE,
                                                  "Avahi resolver failure: %s",
                                                  avahi_strerror(avahi_client_errno(d->client)));
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);
            
            gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
            break;
        }
    }
}


static void browse_callback(
        AvahiServiceBrowser *b,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name,
        const char *type,
        const char *domain,
        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
        void* userdata) {

    AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);
    
    switch (event) {

        case AVAHI_BROWSER_NEW: {
            gchar *ifs;
            const gchar *pretty_type;
            char ifname[IFNAMSIZ];
            GtkTreeIter iter;
            GtkTreeSelection *selection;

            if (!(if_indextoname(interface, ifname)))
                g_snprintf(ifname, sizeof(ifname), "%i", interface);

            ifs = g_strdup_printf("%s %s", ifname, protocol == AVAHI_PROTO_INET ? "IPv4" : "IPv6");

#if defined(HAVE_GDBM) || defined(HAVE_DBM)
            pretty_type = stdb_lookup(type);
#else
            pretty_type = type;
#endif            
            
            gtk_list_store_append(d->service_list_store, &iter);

            gtk_list_store_set(d->service_list_store, &iter,
                               SERVICE_COLUMN_IFACE, interface,
                               SERVICE_COLUMN_PROTO, protocol,
                               SERVICE_COLUMN_NAME, name,
                               SERVICE_COLUMN_TYPE, type,
                               SERVICE_COLUMN_PRETTY_IFACE, ifs,
                               SERVICE_COLUMN_PRETTY_TYPE, pretty_type,
                               -1);

            g_free(ifs);
                    
            if (d->common_protocol == AVAHI_PROTO_UNSPEC)
                d->common_protocol = protocol;

            if (d->common_interface == AVAHI_IF_UNSPEC)
                d->common_interface = interface;

            if (d->common_interface != interface || d->common_protocol != protocol) {
                gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(d->service_tree_view), 0), TRUE);
                gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d->service_tree_view), TRUE);
            }

            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(d->service_tree_view));
            if (!gtk_tree_selection_get_selected(selection, NULL, NULL)) {

                if (!d->service_type ||
                    !d->service_name ||
                    (avahi_domain_equal(d->service_type, type) && strcasecmp(d->service_name, name) == 0)) {
                    GtkTreePath *path;
                    
                    gtk_tree_selection_select_iter(selection, &iter);

                    path = gtk_tree_model_get_path(GTK_TREE_MODEL(d->service_list_store), &iter);
                    gtk_tree_view_set_cursor(GTK_TREE_VIEW(d->service_tree_view), path, NULL, FALSE);
                    gtk_tree_path_free(path);
                }
                
            }
            
            break;
        }
            
        case AVAHI_BROWSER_REMOVE: {
            GtkTreeIter iter;
            gboolean valid;

            valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(d->service_list_store), &iter);
            while (valid) {
                gint _interface, _protocol;
                gchar *_name, *_type;
                gboolean found;
                
                gtk_tree_model_get(GTK_TREE_MODEL(d->service_list_store), &iter,
                                   SERVICE_COLUMN_IFACE, &_interface,
                                   SERVICE_COLUMN_PROTO, &_protocol,
                                   SERVICE_COLUMN_NAME, &_name,
                                   SERVICE_COLUMN_TYPE, &_type,
                                   -1);

                found = _interface == interface && _protocol == protocol && strcasecmp(_name, name) == 0 && avahi_domain_equal(_type, type);
                g_free(_name);

                if (found) {
                    gtk_list_store_remove(d->service_list_store, &iter);
                    break;
                }
                
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(d->service_list_store), &iter);
            }
            
            break;
        }

        case AVAHI_BROWSER_FAILURE: {
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_CLOSE,
                                                  "Browsing for service type %s in domain %s failed: %s",
                                                  type, domain,
                                                  avahi_strerror(avahi_client_errno(d->client)));
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);

            /* Fall through */
        }

        case AVAHI_BROWSER_ALL_FOR_NOW:
            if (d->service_pulse_timeout > 0) {
                g_source_remove(d->service_pulse_timeout);
                d->service_pulse_timeout = 0;
                gtk_widget_hide(d->service_progress_bar);
            }
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            ;
    }
}

static void domain_make_default_selection(AuiServiceDialog *d, const gchar *name, GtkTreeIter *iter) {
    GtkTreeSelection *selection;
    
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(d->domain_tree_view));
    if (!gtk_tree_selection_get_selected(selection, NULL, NULL)) {
        
        if (avahi_domain_equal(gtk_entry_get_text(GTK_ENTRY(d->domain_entry)), name)) {
            GtkTreePath *path;
            
            gtk_tree_selection_select_iter(selection, iter);
            
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(d->domain_list_store), iter);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(d->domain_tree_view), path, NULL, FALSE);
            gtk_tree_path_free(path);
        }
        
    }
}

static void domain_browse_callback(
        AvahiDomainBrowser *b,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name,
        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
        void* userdata) {

    AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);
    
    switch (event) {

        case AVAHI_BROWSER_NEW: {
            GtkTreeIter iter;
            gboolean found = FALSE, valid;
            gint ref;

            valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(d->domain_list_store), &iter);
            while (valid) {
                gchar *_name;
                
                gtk_tree_model_get(GTK_TREE_MODEL(d->domain_list_store), &iter,
                                   DOMAIN_COLUMN_NAME, &_name,
                                   DOMAIN_COLUMN_REF, &ref,
                                   -1);

                found = avahi_domain_equal(_name, name);
                g_free(_name);

                if (found)
                    break;
                
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(d->domain_list_store), &iter);
            }

            if (found) 
                gtk_list_store_set(d->domain_list_store, &iter, DOMAIN_COLUMN_REF, ref + 1, -1);
            else {
                gtk_list_store_append(d->domain_list_store, &iter);
                
                gtk_list_store_set(d->domain_list_store, &iter,
                                   DOMAIN_COLUMN_NAME, name,
                                   DOMAIN_COLUMN_REF, 1,
                                   -1);
            }

            domain_make_default_selection(d, name, &iter);
            
            break;
        }

        case AVAHI_BROWSER_REMOVE: {
            gboolean valid;
            GtkTreeIter iter;
            
            valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(d->domain_list_store), &iter);
            while (valid) {
                gint ref;
                gchar *_name;
                gboolean found;
                
                gtk_tree_model_get(GTK_TREE_MODEL(d->domain_list_store), &iter,
                                   DOMAIN_COLUMN_NAME, &_name,
                                   DOMAIN_COLUMN_REF, &ref,
                                   -1);

                found = avahi_domain_equal(_name, name);
                g_free(_name);

                if (found) {
                    if (ref <= 1)
                        gtk_list_store_remove(d->service_list_store, &iter);
                    else
                        gtk_list_store_set(d->domain_list_store, &iter, DOMAIN_COLUMN_REF, ref - 1, -1);
                    break;
                }
                
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(d->domain_list_store), &iter);
            }
            
            break;
        }
            

        case AVAHI_BROWSER_FAILURE: {
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_CLOSE,
                                                  "Avahi domain browser failure: %s",
                                                  avahi_strerror(avahi_client_errno(d->client)));
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);

            /* Fall through */
        }

        case AVAHI_BROWSER_ALL_FOR_NOW:
            if (d->domain_pulse_timeout > 0) {
                g_source_remove(d->domain_pulse_timeout);
                d->domain_pulse_timeout = 0;
                gtk_widget_hide(d->domain_progress_bar);
            }
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            ;
    }
}

static const gchar *get_domain_name(AuiServiceDialog *d) {
    const gchar *domain;
    
    g_return_val_if_fail(d, NULL);
    
    if (d->domain)
        return d->domain;

    if (!(domain = avahi_client_get_domain_name(d->client))) {
        GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE,
                                              "Failed to read Avahi domain : %s",
                                              avahi_strerror(avahi_client_errno(d->client)));
        gtk_dialog_run(GTK_DIALOG(m));
        gtk_widget_destroy(m);

        return NULL;
    }

    return domain;
}

static gboolean start_callback(gpointer data) {
    int error;
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(data);
    gchar **st;
    AvahiServiceBrowser **sb;
    unsigned i;
    const char *domain;

    d->start_idle = 0;
    
    if (!d->browse_service_types || !*d->browse_service_types) {
        g_warning("Browse service type list is empty!");
        return FALSE;
    }

    if (!d->client) {
        if (!(d->client = avahi_client_new(avahi_glib_poll_get(d->glib_poll), 0, client_callback, d, &error))) {
            
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_CLOSE,
                                                  "Failed to connect to Avahi server: %s",
                                                  avahi_strerror(error));
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);
            
            gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
            return FALSE;
        }
    }

    if (!(domain = get_domain_name(d))) {
        gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
        return FALSE;
    }

    g_assert(domain);

    if (avahi_domain_equal(domain, "local."))
        gtk_label_set_markup(GTK_LABEL(d->domain_label), "Browsing for services on <b>local network</b>:");
    else {
        gchar *t = g_strdup_printf("Browsing for services in domain <b>%s</b>:", domain);
        gtk_label_set_markup(GTK_LABEL(d->domain_label), t);
        g_free(t);
    }
    
    if (d->browsers) {
        for (sb = d->browsers; *sb; sb++)
            avahi_service_browser_free(*sb);

        g_free(d->browsers);
        d->browsers = NULL;
    }

    gtk_list_store_clear(GTK_LIST_STORE(d->service_list_store));
    d->common_interface = AVAHI_IF_UNSPEC;
    d->common_protocol = AVAHI_PROTO_UNSPEC;

    gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(d->service_tree_view), 0), FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d->service_tree_view), gtk_tree_view_column_get_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(d->service_tree_view), 2)));
    gtk_widget_show(d->service_progress_bar);

    if (d->service_pulse_timeout <= 0)
        d->service_pulse_timeout = g_timeout_add(100, service_pulse_callback, d);

    for (i = 0; d->browse_service_types[i]; i++)
        ;
    g_assert(i > 0);

    d->browsers = g_new0(AvahiServiceBrowser*, i+1);
    for (st = d->browse_service_types, sb = d->browsers; *st; st++, sb++) {

        if (!(*sb = avahi_service_browser_new(d->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, *st, d->domain, 0, browse_callback, d))) {
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_CLOSE,
                                                  "Failed to create browser for %s: %s",
                                                  *st,
                                                  avahi_strerror(avahi_client_errno(d->client)));
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);
            
            gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
            return FALSE;

        }
    }

    return FALSE;
}

static void aui_service_dialog_finalize(GObject *object) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(object);

    if (d->domain_pulse_timeout > 0)
        g_source_remove(d->domain_pulse_timeout);

    if (d->service_pulse_timeout > 0)
        g_source_remove(d->service_pulse_timeout);

    if (d->start_idle > 0)
        g_source_remove(d->start_idle);
    
    g_free(d->host_name);
    g_free(d->domain);
    g_free(d->service_name);

    avahi_string_list_free(d->txt_data);
    
    g_strfreev(d->browse_service_types);

    if (d->domain_browser)
        avahi_domain_browser_free(d->domain_browser);
    
    if (d->resolver)
        avahi_service_resolver_free(d->resolver);
    
    if (d->browsers) {
        AvahiServiceBrowser **sb;

        for (sb = d->browsers; *sb; sb++)
            avahi_service_browser_free(*sb);

        g_free(d->browsers);
    }

    if (d->client)
        avahi_client_free(d->client);

    if (d->glib_poll)
        avahi_glib_poll_free(d->glib_poll);

    G_OBJECT_CLASS(aui_service_dialog_parent_class)->finalize(object);
}

static void service_row_activated_callback(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);

    gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
}

static void service_selection_changed_callback(GtkTreeSelection *selection, gpointer user_data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);

    gtk_widget_set_sensitive(d->service_ok_button, gtk_tree_selection_get_selected(selection, NULL, NULL));
}

static void response_callback(GtkDialog *dialog, gint response, gpointer user_data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);

    if (response == GTK_RESPONSE_OK &&
        ((d->resolve_service && !d->resolve_service_done) ||
         (d->resolve_host_name && !d->resolve_host_name_done))) {
        
        GtkTreeIter iter;
        gint interface, protocol;
        gchar *name, *type;
        GdkCursor *cursor;

        g_signal_stop_emission(dialog, g_signal_lookup("response", gtk_dialog_get_type()), 0);

        if (d->resolver)
            return;

        g_return_if_fail(gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(d->service_tree_view)), NULL, &iter));

        gtk_tree_model_get(GTK_TREE_MODEL(d->service_list_store), &iter,
                           SERVICE_COLUMN_IFACE, &interface,
                           SERVICE_COLUMN_PROTO, &protocol,
                           SERVICE_COLUMN_NAME, &name,
                           SERVICE_COLUMN_TYPE, &type, -1);

        g_return_if_fail(d->client);

        gtk_widget_set_sensitive(GTK_WIDGET(dialog), FALSE);
        cursor = gdk_cursor_new(GDK_WATCH);
        gdk_window_set_cursor(GTK_WIDGET(dialog)->window, cursor);
        gdk_cursor_unref(cursor);

        if (!(d->resolver = avahi_service_resolver_new(
                      d->client, interface, protocol, name, type, d->domain,
                      d->address_family, !d->resolve_host_name ? AVAHI_LOOKUP_NO_ADDRESS : 0, resolve_callback, d))) {

            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_CLOSE,
                                                  "Failed to create resolver for %s of type %s in domain %s: %s",
                                                  name, type, d->domain,
                                                  avahi_strerror(avahi_client_errno(d->client)));
            gtk_dialog_run(GTK_DIALOG(m));
            gtk_widget_destroy(m);
            
            gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
            return;
        }
    }
}

static gboolean is_valid_domain_suffix(const gchar *n) {
    gchar label[AVAHI_LABEL_MAX];
    
    if (!avahi_is_valid_domain_name(n))
        return FALSE;

    if (!avahi_unescape_label(&n, label, sizeof(label)))
        return FALSE;

    /* At least one label */
    
    return !!label[0];
}

static void domain_row_activated_callback(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);

    if (is_valid_domain_suffix(gtk_entry_get_text(GTK_ENTRY(d->domain_entry))))
        gtk_dialog_response(GTK_DIALOG(d->domain_dialog), GTK_RESPONSE_OK);
}

static void domain_selection_changed_callback(GtkTreeSelection *selection, gpointer user_data) {
    GtkTreeIter iter;
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
    gchar *name;

    g_return_if_fail(gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(d->domain_tree_view)), NULL, &iter));

    gtk_tree_model_get(GTK_TREE_MODEL(d->domain_list_store), &iter,
                       DOMAIN_COLUMN_NAME, &name, -1);

    gtk_entry_set_text(GTK_ENTRY(d->domain_entry), name);
}

static void domain_entry_changed_callback(GtkEditable *editable, gpointer user_data) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);

    gtk_widget_set_sensitive(d->domain_ok_button, is_valid_domain_suffix(gtk_entry_get_text(GTK_ENTRY(d->domain_entry))));
}

static void domain_button_clicked(GtkButton *button,  gpointer user_data) {
    GtkWidget *vbox, *vbox2, *scrolled_window;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
    const gchar *domain;
    GtkTreeIter iter;

    g_return_if_fail(!d->domain_dialog);
    g_return_if_fail(!d->domain_browser);

    if (!(domain = get_domain_name(d))) {
        gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
        return;
    }

    if (!(d->domain_browser = avahi_domain_browser_new(d->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, NULL, AVAHI_DOMAIN_BROWSER_BROWSE, 0, domain_browse_callback, d))) {
        GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(d),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE,
                                              "Failed to create domain browser: %s",
                                              avahi_strerror(avahi_client_errno(d->client)));
        gtk_dialog_run(GTK_DIALOG(m));
        gtk_widget_destroy(m);
        
        gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
        return;
    }
    
    d->domain_dialog = gtk_dialog_new();
    gtk_container_set_border_width(GTK_CONTAINER(d->domain_dialog), 5);
    gtk_window_set_title(GTK_WINDOW(d->domain_dialog), "Change domain");
    gtk_dialog_set_has_separator(GTK_DIALOG(d->domain_dialog), FALSE);
    
    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d->domain_dialog)->vbox), vbox, TRUE, TRUE, 0);

    d->domain_entry = gtk_entry_new_with_max_length(AVAHI_DOMAIN_NAME_MAX);
    gtk_entry_set_text(GTK_ENTRY(d->domain_entry), domain);
    gtk_entry_set_activates_default(GTK_ENTRY(d->domain_entry), TRUE);
    g_signal_connect(d->domain_entry, "changed", G_CALLBACK(domain_entry_changed_callback), d);
    gtk_box_pack_start(GTK_BOX(vbox), d->domain_entry, FALSE, FALSE, 0);
    
    vbox2 = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(vbox2), scrolled_window, TRUE, TRUE, 0);

    d->domain_list_store = gtk_list_store_new(N_DOMAIN_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

    d->domain_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(d->domain_list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d->domain_tree_view), FALSE);
    g_signal_connect(d->domain_tree_view, "row-activated", G_CALLBACK(domain_row_activated_callback), d);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(d->domain_tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect(selection, "changed", G_CALLBACK(domain_selection_changed_callback), d);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", DOMAIN_COLUMN_NAME, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(d->domain_tree_view), column);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(d->domain_tree_view), DOMAIN_COLUMN_NAME);
    gtk_container_add(GTK_CONTAINER(scrolled_window), d->domain_tree_view);

    d->domain_progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(d->domain_progress_bar), "Browsing ...");
    gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(d->domain_progress_bar), 0.1);
    gtk_box_pack_end(GTK_BOX(vbox2), d->domain_progress_bar, FALSE, FALSE, 0);
    
    gtk_dialog_add_button(GTK_DIALOG(d->domain_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    d->domain_ok_button = GTK_WIDGET(gtk_dialog_add_button(GTK_DIALOG(d->domain_dialog), GTK_STOCK_OK, GTK_RESPONSE_OK));
    gtk_dialog_set_default_response(GTK_DIALOG(d->domain_dialog), GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(d->domain_ok_button, is_valid_domain_suffix(gtk_entry_get_text(GTK_ENTRY(d->domain_entry))));

    gtk_widget_grab_default(d->domain_ok_button);
    gtk_widget_grab_focus(d->domain_entry);

    gtk_window_set_default_size(GTK_WINDOW(d->domain_dialog), 300, 300);
    
    gtk_widget_show_all(vbox);

    gtk_list_store_append(d->domain_list_store, &iter);
    gtk_list_store_set(d->domain_list_store, &iter, DOMAIN_COLUMN_NAME, "local", DOMAIN_COLUMN_REF, 1, -1);
    domain_make_default_selection(d, "local", &iter);

    d->domain_pulse_timeout = g_timeout_add(100, domain_pulse_callback, d);
    
    if (gtk_dialog_run(GTK_DIALOG(d->domain_dialog)) == GTK_RESPONSE_OK)
        aui_service_dialog_set_domain(d, gtk_entry_get_text(GTK_ENTRY(d->domain_entry)));

    gtk_widget_destroy(d->domain_dialog);
    d->domain_dialog = NULL;

    if (d->domain_pulse_timeout > 0) {
        g_source_remove(d->domain_pulse_timeout);
        d->domain_pulse_timeout = 0;
    }

    avahi_domain_browser_free(d->domain_browser);
    d->domain_browser = NULL;

}

static void aui_service_dialog_init(AuiServiceDialog *d) {
    GtkWidget *vbox, *vbox2, *hbox, *scrolled_window;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    d->host_name = NULL;
    d->domain = NULL;
    d->service_name = NULL;
    d->txt_data = NULL;
    d->browse_service_types = NULL;
    memset(&d->address, 0, sizeof(d->address));
    d->port = 0;
    d->resolve_host_name = d->resolve_service = TRUE;
    d->resolve_host_name_done = d->resolve_service_done = FALSE;
    d->address_family = AVAHI_PROTO_UNSPEC;

    d->glib_poll = NULL;
    d->client = NULL;
    d->browsers = NULL;
    d->resolver = NULL;
    d->domain_browser = NULL;

    d->service_pulse_timeout = 0;
    d->domain_pulse_timeout = 0;
    d->start_idle = 0;
    d->common_interface = AVAHI_IF_UNSPEC;
    d->common_protocol = AVAHI_PROTO_UNSPEC;

    d->domain_dialog = NULL;
    d->domain_entry = NULL;
    d->domain_tree_view = NULL;
    d->domain_progress_bar = NULL;
    d->domain_ok_button = NULL;

    d->service_list_store = d->domain_list_store = NULL;
    
    gtk_widget_push_composite_child();

    gtk_container_set_border_width(GTK_CONTAINER(d), 5);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox), vbox, TRUE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    d->domain_label = gtk_label_new("Initializing...");
    gtk_label_set_ellipsize(GTK_LABEL(d->domain_label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(d->domain_label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), d->domain_label, TRUE, TRUE, 0);
    
    d->domain_button = gtk_button_new_with_mnemonic("Change _domain...");
    g_signal_connect(d->domain_button, "clicked", G_CALLBACK(domain_button_clicked), d);
    gtk_box_pack_end(GTK_BOX(hbox), d->domain_button, FALSE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(vbox2), scrolled_window, TRUE, TRUE, 0);

    d->service_list_store = gtk_list_store_new(N_SERVICE_COLUMNS, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    d->service_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(d->service_list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d->service_tree_view), FALSE);
    g_signal_connect(d->service_tree_view, "row-activated", G_CALLBACK(service_row_activated_callback), d);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(d->service_tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect(selection, "changed", G_CALLBACK(service_selection_changed_callback), d);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Location", renderer, "text", SERVICE_COLUMN_PRETTY_IFACE, NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(d->service_tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", SERVICE_COLUMN_NAME, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(d->service_tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", SERVICE_COLUMN_PRETTY_TYPE, NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(d->service_tree_view), column);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(d->service_tree_view), SERVICE_COLUMN_NAME);
    gtk_container_add(GTK_CONTAINER(scrolled_window), d->service_tree_view);

    d->service_progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(d->service_progress_bar), "Browsing ...");
    gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(d->service_progress_bar), 0.1);
    gtk_box_pack_end(GTK_BOX(vbox2), d->service_progress_bar, FALSE, FALSE, 0);
    
    gtk_dialog_add_button(GTK_DIALOG(d), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    d->service_ok_button = GTK_WIDGET(gtk_dialog_add_button(GTK_DIALOG(d), GTK_STOCK_OK, GTK_RESPONSE_OK));
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(d->service_ok_button, FALSE);

    gtk_widget_grab_default(d->service_ok_button);
    gtk_widget_grab_focus(d->service_tree_view);

    gtk_window_set_default_size(GTK_WINDOW(d), 400, 300);
    
    gtk_widget_show_all(vbox);

    gtk_widget_pop_composite_child();

    d->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);

    d->service_pulse_timeout = g_timeout_add(100, service_pulse_callback, d);
    d->start_idle = g_idle_add(start_callback, d);

    g_signal_connect(d, "response", G_CALLBACK(response_callback), d);
}

static void restart_browsing(AuiServiceDialog *d) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));

    if (d->start_idle <= 0)
        d->start_idle = g_idle_add(start_callback, d);
}

void aui_service_dialog_set_browse_service_types(AuiServiceDialog *d, const char *type, ...) {
    va_list ap, apcopy;
    const char *t;
    unsigned u;
    
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
    g_return_if_fail(type);

    g_strfreev(d->browse_service_types);
    
    va_copy(apcopy, ap);
    
    va_start(ap, type);
    for (u = 1; va_arg(ap, const char *); u++)
        ;
    va_end(ap);

    d->browse_service_types = g_new0(gchar*, u+1);
    d->browse_service_types[0] = g_strdup(type);
    
    va_start(apcopy, type);
    for (u = 1; (t = va_arg(apcopy, const char*)); u++)
        d->browse_service_types[u] = g_strdup(t);
    va_end(apcopy);

    if (d->browse_service_types[0] && d->browse_service_types[1]) {
        /* Multiple service types, enable headers */
    
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d->service_tree_view), TRUE);
        gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(d->service_tree_view), 2), TRUE);
    }

    restart_browsing(d);
}

void aui_service_dialog_set_browse_service_typesv(AuiServiceDialog *d, const char *const*types) {
    
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
    g_return_if_fail(types);
    g_return_if_fail(*types);

    g_strfreev(d->browse_service_types);
    d->browse_service_types = g_strdupv((char**) types);

    if (d->browse_service_types[0] && d->browse_service_types[1]) {
        /* Multiple service types, enable headers */
    
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d->service_tree_view), TRUE);
        gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(d->service_tree_view), 2), TRUE);
    }

    restart_browsing(d);
}

const gchar*const* aui_service_dialog_get_browse_service_types(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);

    return (const char* const*) d->browse_service_types;
}

void aui_service_dialog_set_domain(AuiServiceDialog *d, const char *domain) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
    g_return_if_fail(!domain || is_valid_domain_suffix(domain));

    g_free(d->domain);
    d->domain = domain ? avahi_normalize_name_strdup(domain) : NULL;
    
    restart_browsing(d);
}

const char* aui_service_dialog_get_domain(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
    
    return d->domain;
}

void aui_service_dialog_set_service_name(AuiServiceDialog *d, const char *name) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));

    g_free(d->service_name);
    d->service_name = g_strdup(name);
}

const char* aui_service_dialog_get_service_name(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);

    return d->service_name;
}

void aui_service_dialog_set_service_type(AuiServiceDialog *d, const char*stype) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));

    g_free(d->service_type);
    d->service_type = g_strdup(stype);
}

const char* aui_service_dialog_get_service_type(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);

    return d->service_type;
}

const AvahiAddress* aui_service_dialog_get_address(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
    g_return_val_if_fail(d->resolve_service_done && d->resolve_host_name_done, NULL);

    return &d->address;
}

guint16 aui_service_dialog_get_port(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), 0);
    g_return_val_if_fail(d->resolve_service_done, 0);

    return d->port;
}

const char* aui_service_dialog_get_host_name(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
    g_return_val_if_fail(d->resolve_service_done, NULL);

    return d->host_name;
}

const AvahiStringList *aui_service_dialog_get_txt_data(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
    g_return_val_if_fail(d->resolve_service_done, NULL);

    return d->txt_data;
}

void aui_service_dialog_set_resolve_service(AuiServiceDialog *d, gboolean resolve) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
    
    d->resolve_service = resolve;
}

gboolean aui_service_dialog_get_resolve_service(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), FALSE);

    return d->resolve_service;
}

void aui_service_dialog_set_resolve_host_name(AuiServiceDialog *d, gboolean resolve) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
    
    d->resolve_host_name = resolve;
}

gboolean aui_service_dialog_get_resolve_host_name(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), FALSE);

    return d->resolve_host_name;
}

void aui_service_dialog_set_address_family(AuiServiceDialog *d, AvahiProtocol proto) {
    g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
    g_return_if_fail(proto == AVAHI_PROTO_UNSPEC || proto == AVAHI_PROTO_INET || proto == AVAHI_PROTO_INET6);

    d->address_family = proto;
}

AvahiProtocol aui_service_dialog_get_address_family(AuiServiceDialog *d) {
    g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), AVAHI_PROTO_UNSPEC);

    return d->address_family;
}

static void aui_service_dialog_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(object);
    
    switch (prop_id) {
        case PROP_BROWSE_SERVICE_TYPES:
            aui_service_dialog_set_browse_service_typesv(d, g_value_get_pointer(value));
            break;

        case PROP_DOMAIN:
            aui_service_dialog_set_domain(d, g_value_get_string(value));
            break;

        case PROP_SERVICE_TYPE:
            aui_service_dialog_set_service_type(d, g_value_get_string(value));
            break;

        case PROP_SERVICE_NAME:
            aui_service_dialog_set_service_name(d, g_value_get_string(value));
            break;
            
        case PROP_RESOLVE_SERVICE:
            aui_service_dialog_set_resolve_service(d, g_value_get_boolean(value));
            break;

        case PROP_RESOLVE_HOST_NAME:
            aui_service_dialog_set_resolve_host_name(d, g_value_get_boolean(value));
            break;

        case PROP_ADDRESS_FAMILY:
            aui_service_dialog_set_address_family(d, g_value_get_int(value));
            break;
            
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void aui_service_dialog_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    AuiServiceDialog *d = AUI_SERVICE_DIALOG(object);
    
    switch (prop_id) {
        case PROP_BROWSE_SERVICE_TYPES:
            g_value_set_pointer(value, (gpointer) aui_service_dialog_get_browse_service_types(d));
            break;

        case PROP_DOMAIN:
            g_value_set_string(value, aui_service_dialog_get_domain(d));
            break;

        case PROP_SERVICE_TYPE:
            g_value_set_string(value, aui_service_dialog_get_service_type(d));
            break;

        case PROP_SERVICE_NAME:
            g_value_set_string(value, aui_service_dialog_get_service_name(d));
            break;

        case PROP_ADDRESS:
            g_value_set_pointer(value, (gpointer) aui_service_dialog_get_address(d));
            break;

        case PROP_PORT:
            g_value_set_uint(value, aui_service_dialog_get_port(d));
            break;

        case PROP_HOST_NAME:
            g_value_set_string(value, aui_service_dialog_get_host_name(d));
            break;
                               
        case PROP_TXT_DATA:
            g_value_set_pointer(value, (gpointer) aui_service_dialog_get_txt_data(d));
            break;
            
        case PROP_RESOLVE_SERVICE:
            g_value_set_boolean(value, aui_service_dialog_get_resolve_service(d));
            break;

        case PROP_RESOLVE_HOST_NAME:
            g_value_set_boolean(value, aui_service_dialog_get_resolve_host_name(d));
            break;

        case PROP_ADDRESS_FAMILY:
            g_value_set_int(value, aui_service_dialog_get_address_family(d));
            break;
            
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


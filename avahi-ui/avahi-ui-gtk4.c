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

/* GTK4 implementation of the avahi-ui API.
 * Uses GtkListView + GListStore, async dialogs, and GTK4-only APIs. */

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
#include <avahi-common/i18n.h>

#include "avahi-ui.h"

#if defined(HAVE_GDBM)
#include "../avahi-utils/stdb.h"
#endif

/* --- ServiceRow GObject --- */
#define AUI_TYPE_SERVICE_ROW (aui_service_row_get_type())
G_DECLARE_FINAL_TYPE(AuiServiceRow, aui_service_row, AUI, SERVICE_ROW, GObject)

struct _AuiServiceRow {
	GObject parent_instance;
	int iface;
	int protocol;
	char *name;
	char *type;
	char *pretty_iface;
	char *pretty_type;
};

G_DEFINE_TYPE(AuiServiceRow, aui_service_row, G_TYPE_OBJECT)

static void aui_service_row_finalize(GObject *object)
{
	AuiServiceRow *r = AUI_SERVICE_ROW(object);
	g_free(r->name);
	g_free(r->type);
	g_free(r->pretty_iface);
	g_free(r->pretty_type);
	G_OBJECT_CLASS(aui_service_row_parent_class)->finalize(object);
}

static void aui_service_row_class_init(AuiServiceRowClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = aui_service_row_finalize;
}

static void aui_service_row_init(AuiServiceRow *self)
{
	(void)self;
}

/* --- DomainRow GObject --- */
#define AUI_TYPE_DOMAIN_ROW (aui_domain_row_get_type())
G_DECLARE_FINAL_TYPE(AuiDomainRow, aui_domain_row, AUI, DOMAIN_ROW, GObject)

struct _AuiDomainRow {
	GObject parent_instance;
	char *name;
	int ref;
};

G_DEFINE_TYPE(AuiDomainRow, aui_domain_row, G_TYPE_OBJECT)

static void aui_domain_row_finalize(GObject *object)
{
	AuiDomainRow *r = AUI_DOMAIN_ROW(object);
	g_free(r->name);
	G_OBJECT_CLASS(aui_domain_row_parent_class)->finalize(object);
}

static void aui_domain_row_class_init(AuiDomainRowClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = aui_domain_row_finalize;
}

static void aui_domain_row_init(AuiDomainRow *self)
{
	(void)self;
}

struct _AuiServiceDialogPrivate {
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
	GtkWidget *service_list_view;
	GtkWidget *service_progress_bar;

	GListStore *service_list_store;
	GListStore *domain_list_store;
	GHashTable *service_type_names;

	guint service_pulse_timeout;
	guint domain_pulse_timeout;
	guint start_idle;

	AvahiIfIndex common_interface;
	AvahiProtocol common_protocol;

	GtkWidget *domain_dialog;
	GtkWidget *domain_entry;
	GtkWidget *domain_list_view;
	GtkWidget *domain_progress_bar;
	GtkWidget *domain_ok_button;

	gint forward_response_id;
	gboolean type_column_visible;
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

static void aui_service_dialog_finalize(GObject *object);
static void aui_service_dialog_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void aui_service_dialog_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE_WITH_PRIVATE(AuiServiceDialog, aui_service_dialog, GTK_TYPE_DIALOG)

/* Show error dialog; on close, destroy it and emit response on main dialog */
static void error_dialog_response_cb(GtkDialog *err_dlg, gint response_id G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = user_data;
	gtk_window_destroy(GTK_WINDOW(err_dlg));
	gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
}

static void show_error_dialog(GtkWindow *parent, const char *msg)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(parent);
	GtkWidget *m = gtk_message_dialog_new(parent,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE,
		"%s", msg);
	g_signal_connect(m, "response", G_CALLBACK(error_dialog_response_cb), d);
	gtk_widget_set_visible(m, TRUE);
}

static gint get_default_response(GtkDialog *dlg)
{
	GtkWidget *def = gtk_window_get_default_widget(GTK_WINDOW(dlg));
	if (def)
		return gtk_dialog_get_response_for_widget(dlg, def);
	/* First positive response */
	return GTK_RESPONSE_ACCEPT;
}

static void aui_service_dialog_class_init(AuiServiceDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	avahi_init_i18n();

	object_class->finalize = aui_service_dialog_finalize;
	object_class->set_property = aui_service_dialog_set_property;
	object_class->get_property = aui_service_dialog_get_property;

	g_object_class_install_property(object_class, PROP_BROWSE_SERVICE_TYPES,
		g_param_spec_pointer("browse_service_types", NULL, NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(object_class, PROP_DOMAIN,
		g_param_spec_string("domain", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(object_class, PROP_SERVICE_TYPE,
		g_param_spec_string("service_type", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(object_class, PROP_SERVICE_NAME,
		g_param_spec_string("service_name", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(object_class, PROP_ADDRESS,
		g_param_spec_pointer("address", NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property(object_class, PROP_PORT,
		g_param_spec_uint("port", NULL, NULL, 0, 0xFFFF, 0, G_PARAM_READABLE));
	g_object_class_install_property(object_class, PROP_HOST_NAME,
		g_param_spec_string("host_name", NULL, NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property(object_class, PROP_TXT_DATA,
		g_param_spec_pointer("txt_data", NULL, NULL, G_PARAM_READABLE));
	g_object_class_install_property(object_class, PROP_RESOLVE_SERVICE,
		g_param_spec_boolean("resolve_service", NULL, NULL, TRUE, G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(object_class, PROP_RESOLVE_HOST_NAME,
		g_param_spec_boolean("resolve_host_name", NULL, NULL, TRUE, G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property(object_class, PROP_ADDRESS_FAMILY,
		g_param_spec_int("address_family", NULL, NULL, AVAHI_PROTO_UNSPEC, AVAHI_PROTO_INET6, AVAHI_PROTO_UNSPEC, G_PARAM_READABLE | G_PARAM_WRITABLE));
}

GtkWidget *aui_service_dialog_new_valist(const gchar *title, GtkWindow *parent,
	const gchar *first_button_text, va_list varargs)
{
	const gchar *button_text;
	GtkWidget *w;

	w = GTK_WIDGET(g_object_new(AUI_TYPE_SERVICE_DIALOG, "title", title, NULL));

	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(w), parent);

	button_text = first_button_text;
	while (button_text) {
		gint response_id = va_arg(varargs, gint);
		gtk_dialog_add_button(GTK_DIALOG(w), button_text, response_id);
		button_text = va_arg(varargs, const gchar *);
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(w), GTK_RESPONSE_ACCEPT, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(w), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(w), GTK_RESPONSE_YES, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(w), GTK_RESPONSE_APPLY, FALSE);

	if (get_default_response(GTK_DIALOG(w)) != GTK_RESPONSE_NONE)
		gtk_dialog_set_default_response(GTK_DIALOG(w), get_default_response(GTK_DIALOG(w)));

	return w;
}

GtkWidget *aui_service_dialog_new(const gchar *title, GtkWindow *parent,
	const gchar *first_button_text, ...)
{
	GtkWidget *w;
	va_list varargs;
	va_start(varargs, first_button_text);
	w = aui_service_dialog_new_valist(title, parent, first_button_text, varargs);
	va_end(varargs);
	return w;
}

static gboolean service_pulse_callback(gpointer data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(p->service_progress_bar));
	return TRUE;
}

static gboolean domain_pulse_callback(gpointer data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(p->domain_progress_bar));
	return TRUE;
}

static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);

	if (state == AVAHI_CLIENT_FAILURE) {
		char *msg = g_strdup_printf(_("Avahi client failure: %s"), avahi_strerror(avahi_client_errno(c)));
		show_error_dialog(GTK_WINDOW(d), msg);
		g_free(msg);
	}
}

static void resolve_callback(AvahiServiceResolver *r G_GNUC_UNUSED,
	AvahiIfIndex i G_GNUC_UNUSED, AvahiProtocol p G_GNUC_UNUSED,
	AvahiResolverEvent event, const char *name, const char *type, const char *domain,
	const char *host_name, const AvahiAddress *a, uint16_t port, const AvahiStringList *txt,
	AvahiLookupResultFlags flags G_GNUC_UNUSED, void *userdata)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);

	AuiServiceDialogPrivate *priv = aui_service_dialog_get_instance_private(d);

	switch (event) {
	case AVAHI_RESOLVER_FOUND:
		priv->resolve_service_done = TRUE;
		g_free(priv->service_name);
		priv->service_name = g_strdup(name);
		g_free(priv->service_type);
		priv->service_type = g_strdup(type);
		g_free(priv->domain);
		priv->domain = g_strdup(domain);
		g_free(priv->host_name);
		priv->host_name = g_strdup(host_name);
		priv->port = port;
		avahi_string_list_free(priv->txt_data);
		priv->txt_data = avahi_string_list_copy(txt);
		if (a) {
			priv->resolve_host_name_done = TRUE;
			priv->address = *a;
		}
		if (priv->resolver) {
			avahi_service_resolver_free(priv->resolver);
			priv->resolver = NULL;
		}
		gtk_dialog_response(GTK_DIALOG(d), priv->forward_response_id);
		break;
	case AVAHI_RESOLVER_FAILURE: {
		char *msg;
		if (priv->resolver) {
			avahi_service_resolver_free(priv->resolver);
			priv->resolver = NULL;
		}
		msg = g_strdup_printf(_("Avahi resolver failure: %s"),
			avahi_strerror(avahi_client_errno(priv->client)));
		show_error_dialog(GTK_WINDOW(d), msg);
		g_free(msg);
		gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
		break;
	}
	}
}

static void browse_callback(AvahiServiceBrowser *b G_GNUC_UNUSED,
	AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event,
	const char *name, const char *type, const char *domain,
	AvahiLookupResultFlags flags G_GNUC_UNUSED, void *userdata)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	gboolean old_column_visible = p->type_column_visible;
	char ifname[IFNAMSIZ];

	switch (event) {
	case AVAHI_BROWSER_NEW: {
		AuiServiceRow *row;
		const gchar *pretty_type = NULL;

		if (!if_indextoname(interface, ifname))
			g_snprintf(ifname, sizeof(ifname), "%i", interface);

		if (p->service_type_names)
			pretty_type = g_hash_table_lookup(p->service_type_names, type);
		if (!pretty_type) {
#if defined(HAVE_GDBM)
			pretty_type = stdb_lookup(type);
#else
			pretty_type = type;
#endif
		}

		row = g_object_new(AUI_TYPE_SERVICE_ROW, NULL);
		row->iface = interface;
		row->protocol = protocol;
		row->name = g_strdup(name);
		row->type = g_strdup(type);
		row->pretty_iface = g_strdup_printf("%s %s", ifname, protocol == AVAHI_PROTO_INET ? "IPv4" : "IPv6");
		row->pretty_type = g_strdup(pretty_type);

		g_list_store_append(p->service_list_store, row);
		g_object_unref(row);

		if (p->common_protocol == AVAHI_PROTO_UNSPEC)
			p->common_protocol = protocol;
		if (p->common_interface == AVAHI_IF_UNSPEC)
			p->common_interface = interface;
		if (p->common_interface != interface || p->common_protocol != protocol)
			p->type_column_visible = TRUE;

		if (!old_column_visible && p->type_column_visible) {
			guint n = g_list_model_get_n_items(G_LIST_MODEL(p->service_list_store));
			if (n > 0) {
				g_list_model_items_changed(G_LIST_MODEL(p->service_list_store), 0, n, n);
			}
		}

		break;
	}
	case AVAHI_BROWSER_REMOVE: {
		guint n, i;
		n = g_list_model_get_n_items(G_LIST_MODEL(p->service_list_store));
		for (i = 0; i < n; i++) {
			AuiServiceRow *row = g_list_model_get_item(G_LIST_MODEL(p->service_list_store), i);
			if (row && row->iface == (int)interface && row->protocol == (int)protocol &&
			    strcasecmp(row->name, name) == 0 && avahi_domain_equal(row->type, type)) {
				g_list_store_remove(p->service_list_store, i);
				g_object_unref(row);
				break;
			}
			if (row)
				g_object_unref(row);
		}
		break;
	}
	case AVAHI_BROWSER_FAILURE: {
		char *msg = g_strdup_printf(_("Browsing for service type %s in domain %s failed: %s"),
			type, domain ? domain : _("n/a"), avahi_strerror(avahi_client_errno(p->client)));
		show_error_dialog(GTK_WINDOW(d), msg);
		g_free(msg);
		G_GNUC_FALLTHROUGH;
		}
	case AVAHI_BROWSER_ALL_FOR_NOW:
		if (p->service_pulse_timeout > 0) {
			g_source_remove(p->service_pulse_timeout);
			p->service_pulse_timeout = 0;
			gtk_widget_set_visible(p->service_progress_bar, FALSE);
		}
		break;
	case AVAHI_BROWSER_CACHE_EXHAUSTED:
		break;
	}
}

static void domain_make_default_selection(AuiServiceDialog *d, const gchar *name)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	/* Select first row if it matches the current domain entry */
	if (p->domain_list_store && g_list_model_get_n_items(G_LIST_MODEL(p->domain_list_store)) > 0 &&
	    avahi_domain_equal(gtk_editable_get_text(GTK_EDITABLE(p->domain_entry)), name)) {
		GtkSingleSelection *sel = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(p->domain_list_view)));
		gtk_single_selection_set_selected(sel, 0);
	}
}

static void domain_browse_callback(AvahiDomainBrowser *b G_GNUC_UNUSED,
	AvahiIfIndex i G_GNUC_UNUSED, AvahiProtocol proto G_GNUC_UNUSED,
	AvahiBrowserEvent event, const char *name,
	AvahiLookupResultFlags flags G_GNUC_UNUSED, void *userdata)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(userdata);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	guint n;
	GListModel *model = G_LIST_MODEL(p->domain_list_store);

	switch (event) {
	case AVAHI_BROWSER_NEW: {
		guint idx;
		n = g_list_model_get_n_items(model);
		for (idx = 0; idx < n; idx++) {
			AuiDomainRow *row = g_list_model_get_item(model, idx);
			if (row && avahi_domain_equal(row->name, name)) {
				row->ref++;
				g_object_unref(row);
				return;
			}
			if (row)
				g_object_unref(row);
		}
		{
			AuiDomainRow *row = g_object_new(AUI_TYPE_DOMAIN_ROW, NULL);
			row->name = g_strdup(name);
			row->ref = 1;
			g_list_store_append(p->domain_list_store, row);
			g_object_unref(row);
			domain_make_default_selection(d, name);
		}
		break;
	}
	case AVAHI_BROWSER_REMOVE: {
		guint idx;
		n = g_list_model_get_n_items(model);
		for (idx = 0; idx < n; idx++) {
			AuiDomainRow *row = g_list_model_get_item(model, idx);
			if (row && avahi_domain_equal(row->name, name)) {
				if (row->ref <= 1) {
					g_list_store_remove(p->domain_list_store, idx);
					g_object_unref(row);
				} else {
					row->ref--;
					g_object_unref(row);
				}
				return;
			}
			if (row)
				g_object_unref(row);
		}
		break;
	}
	case AVAHI_BROWSER_FAILURE: {
		char *msg = g_strdup_printf(_("Avahi domain browser failure: %s"),
			avahi_strerror(avahi_client_errno(p->client)));
		show_error_dialog(GTK_WINDOW(p->domain_dialog), msg);
		g_free(msg);
		G_GNUC_FALLTHROUGH;
		}
	case AVAHI_BROWSER_ALL_FOR_NOW:
		if (p->domain_pulse_timeout > 0) {
			g_source_remove(p->domain_pulse_timeout);
			p->domain_pulse_timeout = 0;
			gtk_widget_set_visible(p->domain_progress_bar, FALSE);
		}
		break;
	case AVAHI_BROWSER_CACHE_EXHAUSTED:
		break;
	}
}

static const gchar *get_domain_name(AuiServiceDialog *d)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	const gchar *domain;

	if (p->domain)
		return p->domain;
	if (!p->client)
		return NULL;
	domain = avahi_client_get_domain_name(p->client);
	if (!domain) {
		char *msg = g_strdup_printf(_("Failed to read Avahi domain: %s"),
			avahi_strerror(avahi_client_errno(p->client)));
		show_error_dialog(GTK_WINDOW(d), msg);
		g_free(msg);
		return NULL;
	}
	return domain;
}

static gboolean start_callback(gpointer data)
{
	int error;
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	gchar **st;
	AvahiServiceBrowser **sb;
	unsigned i;
	const char *domain;

	p->start_idle = 0;

	if (!p->browse_service_types || !*p->browse_service_types) {
		g_warning(_("Browse service type list is empty!"));
		return G_SOURCE_REMOVE;
	}

	if (!p->client) {
		if (!(p->client = avahi_client_new(avahi_glib_poll_get(p->glib_poll), 0, client_callback, d, &error))) {
			char *msg = g_strdup_printf(_("Failed to connect to Avahi server: %s"), avahi_strerror(error));
			show_error_dialog(GTK_WINDOW(d), msg);
			g_free(msg);
			gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
			return G_SOURCE_REMOVE;
		}
	}

	if (!(domain = get_domain_name(d))) {
		gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
		return G_SOURCE_REMOVE;
	}

	if (avahi_domain_equal(domain, "local."))
		gtk_label_set_markup(GTK_LABEL(p->domain_label), _("Browsing for services on <b>local network</b>:"));
	else {
		char *t = g_strdup_printf(_("Browsing for services in domain <b>%s</b>:"), domain);
		gtk_label_set_markup(GTK_LABEL(p->domain_label), t);
		g_free(t);
	}

	if (p->browsers) {
		for (sb = p->browsers; *sb; sb++)
			avahi_service_browser_free(*sb);
		g_free(p->browsers);
		p->browsers = NULL;
	}

	/* Clear service list (GListStore unrefs items automatically) */
	g_list_store_remove_all(p->service_list_store);

	p->common_interface = AVAHI_IF_UNSPEC;
	p->common_protocol = AVAHI_PROTO_UNSPEC;
	p->type_column_visible = FALSE;

	gtk_widget_set_visible(p->service_progress_bar, TRUE);
	if (p->service_pulse_timeout == 0)
		p->service_pulse_timeout = g_timeout_add(100, service_pulse_callback, d);

	for (i = 0; p->browse_service_types[i]; i++)
		;
	p->browsers = g_new0(AvahiServiceBrowser *, i + 1);
	for (st = p->browse_service_types, sb = p->browsers; *st; st++, sb++) {
		if (!(*sb = avahi_service_browser_new(p->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, *st, p->domain, 0, browse_callback, d))) {
			char *msg = g_strdup_printf(_("Failed to create browser for %s: %s"), *st, avahi_strerror(avahi_client_errno(p->client)));
			show_error_dialog(GTK_WINDOW(d), msg);
			g_free(msg);
			gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
			return G_SOURCE_REMOVE;
		}
	}

	return G_SOURCE_REMOVE;
}

static void aui_service_dialog_finalize(GObject *object)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(object);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);

	if (p->domain_pulse_timeout > 0)
		g_source_remove(p->domain_pulse_timeout);
	if (p->service_pulse_timeout > 0)
		g_source_remove(p->service_pulse_timeout);
	if (p->start_idle > 0)
		g_source_remove(p->start_idle);

	g_free(p->host_name);
	g_free(p->domain);
	g_free(p->service_name);
	avahi_string_list_free(p->txt_data);
	g_strfreev(p->browse_service_types);

	if (p->domain_browser)
		avahi_domain_browser_free(p->domain_browser);
	if (p->resolver)
		avahi_service_resolver_free(p->resolver);
	if (p->browsers) {
		for (AvahiServiceBrowser **sb = p->browsers; *sb; sb++)
			avahi_service_browser_free(*sb);
		g_free(p->browsers);
	}
	if (p->client)
		avahi_client_free(p->client);
	if (p->glib_poll)
		avahi_glib_poll_free(p->glib_poll);

	/* Both stores are owned by GtkSingleSelection (transfer-full at gtk_single_selection_new);
	 * do not unref, only clear so we never touch freed memory. */
	p->service_list_store = NULL;
	p->domain_list_store = NULL;
	if (p->service_type_names)
		g_hash_table_unref(p->service_type_names);

	G_OBJECT_CLASS(aui_service_dialog_parent_class)->finalize(object);
}

static void service_activate_cb(GtkListView *list_view G_GNUC_UNUSED, guint position, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	gtk_dialog_response(GTK_DIALOG(d), get_default_response(GTK_DIALOG(d)));
	(void)position;
}

static void service_selection_changed_cb(GObject *selection G_GNUC_UNUSED, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	GtkSingleSelection *sel = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(p->service_list_view)));
	gboolean has_selection = gtk_single_selection_get_selected_item(sel) != NULL;

	gtk_dialog_set_response_sensitive(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT, has_selection);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(d), GTK_RESPONSE_OK, has_selection);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(d), GTK_RESPONSE_YES, has_selection);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(d), GTK_RESPONSE_APPLY, has_selection);
}

static void response_callback(GtkDialog *dialog, gint response, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);

	if ((response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_OK ||
	     response == GTK_RESPONSE_YES || response == GTK_RESPONSE_APPLY) &&
	    ((p->resolve_service && !p->resolve_service_done) ||
	     (p->resolve_host_name && !p->resolve_host_name_done))) {

		GtkSingleSelection *sel = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(p->service_list_view)));
		AuiServiceRow *row = gtk_single_selection_get_selected_item(sel);

		g_signal_stop_emission_by_name(dialog, "response");
		p->forward_response_id = response;

		if (p->resolver)
			return;

		if (!row) return;

		/* Returns a borrowed reference (transfer none); do not unref */
		gtk_widget_set_sensitive(GTK_WIDGET(dialog), FALSE);
		{
			GdkCursor *cursor = gdk_cursor_new_from_name("wait", NULL);
			if (cursor) {
				gtk_widget_set_cursor(GTK_WIDGET(dialog), cursor);
				g_object_unref(cursor);
			}
		}

		if (!(p->resolver = avahi_service_resolver_new(p->client,
			    row->iface, row->protocol, row->name, row->type, p->domain,
			    p->address_family, !p->resolve_host_name ? AVAHI_LOOKUP_NO_ADDRESS : 0,
			    (AvahiServiceResolverCallback) resolve_callback, d))) {
			char *msg = g_strdup_printf(_("Failed to create resolver for %s of type %s in domain %s: %s"),
				row->name, row->type, p->domain, avahi_strerror(avahi_client_errno(p->client)));
			show_error_dialog(GTK_WINDOW(d), msg);
			g_free(msg);
			gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
		}
	}
}

static gboolean is_valid_domain_suffix(const gchar *n)
{
	gchar label[AVAHI_LABEL_MAX];
	if (!avahi_is_valid_domain_name(n)) return FALSE;
	if (!avahi_unescape_label(&n, label, sizeof(label))) return FALSE;
	return !!label[0];
}

static void domain_activate_cb(GtkListView *lv G_GNUC_UNUSED, guint pos G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	if (is_valid_domain_suffix(gtk_editable_get_text(GTK_EDITABLE(p->domain_entry))))
		gtk_dialog_response(GTK_DIALOG(p->domain_dialog), GTK_RESPONSE_ACCEPT);
}

static void domain_selection_changed_cb(GObject *sel G_GNUC_UNUSED, GParamSpec *pspec G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	GtkSingleSelection *single = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(p->domain_list_view)));
	const AuiDomainRow *row = gtk_single_selection_get_selected_item(single);
	/* Returns a borrowed reference (transfer none); do not unref */
	if (row)
		gtk_editable_set_text(GTK_EDITABLE(p->domain_entry), row->name);
}

static void domain_entry_changed_cb(GtkEditable *editable G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	gtk_widget_set_sensitive(p->domain_ok_button, is_valid_domain_suffix(gtk_editable_get_text(GTK_EDITABLE(p->domain_entry))));
}

/* Called when the domain dialog is destroyed (user OK/Cancel or main window closed).
 * domain_list_store is owned by the dialog's GtkSingleSelection; only clear the pointer. */
static void domain_dialog_destroy_cb(GtkWidget *dlg G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);

	if (p->domain_pulse_timeout > 0) {
		g_source_remove(p->domain_pulse_timeout);
		p->domain_pulse_timeout = 0;
	}
	if (p->domain_browser) {
		avahi_domain_browser_free(p->domain_browser);
		p->domain_browser = NULL;
	}
	p->domain_list_store = NULL;
	p->domain_dialog = NULL;
}

static void domain_dialog_response_cb(GtkDialog *dlg, gint response_id, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);

	if (response_id == GTK_RESPONSE_ACCEPT) {
		aui_service_dialog_set_domain(d, gtk_editable_get_text(GTK_EDITABLE(p->domain_entry)));
	}

	gtk_window_destroy(GTK_WINDOW(dlg));
	/* domain_dialog_destroy_cb will run and unref store / clear pointers */

	if (p->domain_pulse_timeout > 0) {
		g_source_remove(p->domain_pulse_timeout);
		p->domain_pulse_timeout = 0;
	}
	if (p->domain_browser) {
		avahi_domain_browser_free(p->domain_browser);
		p->domain_browser = NULL;
	}
	/* domain_list_store is owned by the dialog's GtkSingleSelection; destroy_cb clears the pointer. */
}

/* Service list item factory: create row with labels */
static void service_list_setup_cb(GtkListItemFactory *factory G_GNUC_UNUSED, GtkListItem *item, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
	GtkLabel *l0 = GTK_LABEL(gtk_label_new(NULL));
	GtkLabel *l1 = GTK_LABEL(gtk_label_new(NULL));
	GtkLabel *l2 = GTK_LABEL(gtk_label_new(NULL));

	gtk_widget_set_visible(GTK_WIDGET(l0), p->type_column_visible);
	gtk_label_set_xalign(l0, 0);
	gtk_label_set_xalign(l1, 0);
	gtk_label_set_xalign(l2, 0);
	gtk_label_set_ellipsize(l1, PANGO_ELLIPSIZE_END);
	gtk_box_append(box, GTK_WIDGET(l0));
	gtk_box_append(box, GTK_WIDGET(l1));
	gtk_box_append(box, GTK_WIDGET(l2));
	gtk_list_item_set_child(item, GTK_WIDGET(box));
	gtk_list_item_set_activatable(item, TRUE);
}

static void service_list_bind_cb(GtkListItemFactory *factory G_GNUC_UNUSED, GtkListItem *item, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	GtkWidget *child = gtk_list_item_get_child(item);
	const AuiServiceRow *row = gtk_list_item_get_item(item);
	GtkBox *box;
	GtkLabel *l0, *l1, *l2;

	if (!row || !child) return;

	box = GTK_BOX(child);
	l0 = GTK_LABEL(gtk_widget_get_first_child(GTK_WIDGET(box)));
	l1 = GTK_LABEL(gtk_widget_get_next_sibling(GTK_WIDGET(l0)));
	l2 = GTK_LABEL(gtk_widget_get_next_sibling(GTK_WIDGET(l1)));

	gtk_label_set_text(l0, row->pretty_iface);
	gtk_label_set_text(l1, row->name);
	gtk_label_set_text(l2, row->pretty_type);
	gtk_widget_set_visible(GTK_WIDGET(l0), p->type_column_visible);
	gtk_widget_set_visible(GTK_WIDGET(l2), p->type_column_visible);
}

/* Domain list item factory */
static void domain_list_setup_cb(GtkListItemFactory *f G_GNUC_UNUSED, GtkListItem *item, gpointer user_data G_GNUC_UNUSED)
{
	GtkLabel *l = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_xalign(l, 0);
	gtk_list_item_set_child(item, GTK_WIDGET(l));
	gtk_list_item_set_activatable(item, TRUE);
}

static void domain_list_bind_cb(GtkListItemFactory *f G_GNUC_UNUSED, GtkListItem *item, gpointer u G_GNUC_UNUSED)
{
	const AuiDomainRow *row = gtk_list_item_get_item(item);
	GtkWidget *child = gtk_list_item_get_child(item);
	/* Returns a borrowed reference (transfer none); do not unref */
	if (row && child)
		gtk_label_set_text(GTK_LABEL(child), row->name);
}

static void domain_button_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(user_data);
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	const gchar *domain;
	GtkWidget *vbox, *vbox2, *scrolled;
	GtkSingleSelection *dom_sel;
	GtkListItemFactory *dom_factory;
	AuiDomainRow *row;

	g_return_if_fail(!p->domain_dialog);
	g_return_if_fail(!p->domain_browser);

	if (!(domain = get_domain_name(d))) {
		gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
		return;
	}

	if (!(p->domain_browser = avahi_domain_browser_new(p->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, NULL, AVAHI_DOMAIN_BROWSER_BROWSE, 0, domain_browse_callback, d))) {
		char *msg = g_strdup_printf(_("Failed to create domain browser: %s"), avahi_strerror(avahi_client_errno(p->client)));
		show_error_dialog(GTK_WINDOW(d), msg);
		g_free(msg);
		gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CANCEL);
		return;
	}

	p->domain_dialog = gtk_dialog_new();
	gtk_widget_add_css_class(p->domain_dialog, "aui-dialog");
	gtk_window_set_title(GTK_WINDOW(p->domain_dialog), _("Change domain"));
	gtk_widget_set_margin_start(GTK_WIDGET(p->domain_dialog), 3);
	gtk_widget_set_margin_end(GTK_WIDGET(p->domain_dialog), 3);
	gtk_widget_set_margin_top(GTK_WIDGET(p->domain_dialog), 3);
	gtk_widget_set_margin_bottom(GTK_WIDGET(p->domain_dialog), 3);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_start(vbox, 4);
	gtk_widget_set_margin_end(vbox, 4);
	gtk_widget_set_margin_top(vbox, 4);
	gtk_widget_set_margin_bottom(vbox, 4);
	gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(p->domain_dialog))), vbox);

	p->domain_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(p->domain_entry), AVAHI_DOMAIN_NAME_MAX);
	gtk_editable_set_text(GTK_EDITABLE(p->domain_entry), domain);
	gtk_entry_set_activates_default(GTK_ENTRY(p->domain_entry), TRUE);
	g_signal_connect(p->domain_entry, "changed", G_CALLBACK(domain_entry_changed_cb), d);
	gtk_box_append(GTK_BOX(vbox), p->domain_entry);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_box_append(GTK_BOX(vbox), vbox2);

	scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 200);
	gtk_widget_set_vexpand(scrolled, TRUE);
	gtk_widget_set_hexpand(scrolled, TRUE);
	gtk_box_append(GTK_BOX(vbox2), scrolled);

	p->domain_list_store = g_list_store_new(AUI_TYPE_DOMAIN_ROW);
	dom_sel = gtk_single_selection_new(G_LIST_MODEL(p->domain_list_store));
	dom_factory = gtk_signal_list_item_factory_new();
	g_signal_connect(dom_factory, "setup", G_CALLBACK(domain_list_setup_cb), NULL);
	g_signal_connect(dom_factory, "bind", G_CALLBACK(domain_list_bind_cb), NULL);
	p->domain_list_view = gtk_list_view_new(GTK_SELECTION_MODEL(dom_sel), dom_factory);
	g_signal_connect(p->domain_list_view, "activate", G_CALLBACK(domain_activate_cb), d);
	g_signal_connect(dom_sel, "notify::selected", G_CALLBACK(domain_selection_changed_cb), d);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), p->domain_list_view);

	p->domain_progress_bar = gtk_progress_bar_new();
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(p->domain_progress_bar), _("Browsing..."));
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(p->domain_progress_bar), 0.1);
	gtk_box_append(GTK_BOX(vbox2), p->domain_progress_bar);

	gtk_dialog_add_button(GTK_DIALOG(p->domain_dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	p->domain_ok_button = GTK_WIDGET(gtk_dialog_add_button(GTK_DIALOG(p->domain_dialog), _("_OK"), GTK_RESPONSE_ACCEPT));
	gtk_dialog_set_default_response(GTK_DIALOG(p->domain_dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_set_sensitive(p->domain_ok_button, is_valid_domain_suffix(gtk_editable_get_text(GTK_EDITABLE(p->domain_entry))));

	gtk_window_set_default_size(GTK_WINDOW(p->domain_dialog), 380, 380);

	row = g_object_new(AUI_TYPE_DOMAIN_ROW, NULL);
	row->name = g_strdup("local");
	row->ref = 1;
	g_list_store_append(p->domain_list_store, row);
	g_object_unref(row);
	gtk_single_selection_set_selected(dom_sel, 0);

	p->domain_pulse_timeout = g_timeout_add(100, domain_pulse_callback, d);

	g_signal_connect(p->domain_dialog, "destroy", G_CALLBACK(domain_dialog_destroy_cb), d);
	g_signal_connect(p->domain_dialog, "response", G_CALLBACK(domain_dialog_response_cb), d);
	gtk_widget_set_visible(vbox, TRUE);
	gtk_window_present(GTK_WINDOW(p->domain_dialog));
}

/* Embedded CSS for avahi-ui dialogs (avoids separate .css file and build complexity).
 * Compacts title bar and button area to better match GTK3 spacing. */
static void aui_service_dialog_ensure_css(void)
{
	static gboolean added;
	GdkDisplay *display;
	if (added) return;
	display = gdk_display_get_default();
	if (!display) return;
	added = TRUE;
	{
		GtkCssProvider *provider = gtk_css_provider_new();
		gtk_css_provider_load_from_string(provider,
			".aui-dialog headerbar { min-height: 0; padding: 4px 8px; }\n"
			".aui-dialog headerbar windowcontrols { margin: 0; padding: 0; }\n"
			".aui-dialog box > box:last-child { margin: 4px; }\n"
			".aui-dialog box > box:last-child > button { margin-left: 2px; margin-right: 2px; }\n");
		gtk_style_context_add_provider_for_display(display,
			GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(provider);
	}
}

static void aui_service_dialog_init(AuiServiceDialog *d)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	GtkSingleSelection *sel;
	GtkListItemFactory *factory;
	GtkWidget *vbox, *vbox2, *scrolled;

	aui_service_dialog_ensure_css();
	gtk_widget_add_css_class(GTK_WIDGET(d), "aui-dialog");

	p->service_list_store = g_list_store_new(AUI_TYPE_SERVICE_ROW);
	sel = gtk_single_selection_new(G_LIST_MODEL(p->service_list_store));
	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(service_list_setup_cb), d);
	g_signal_connect(factory, "bind", G_CALLBACK(service_list_bind_cb), d);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_start(vbox, 4);
	gtk_widget_set_margin_end(vbox, 4);
	gtk_widget_set_margin_top(vbox, 4);
	gtk_widget_set_margin_bottom(vbox, 4);
	gtk_widget_set_margin_start(GTK_WIDGET(d), 3);
	gtk_widget_set_margin_end(GTK_WIDGET(d), 3);
	gtk_widget_set_margin_top(GTK_WIDGET(d), 3);
	gtk_widget_set_margin_bottom(GTK_WIDGET(d), 3);

	gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(d))), vbox);

	p->domain_label = gtk_label_new(_("Initializing..."));
	gtk_label_set_ellipsize(GTK_LABEL(p->domain_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_xalign(GTK_LABEL(p->domain_label), 0);
	gtk_label_set_yalign(GTK_LABEL(p->domain_label), 0.5);
	gtk_box_append(GTK_BOX(vbox), p->domain_label);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_box_append(GTK_BOX(vbox), vbox2);

	scrolled = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 250);
	gtk_widget_set_vexpand(scrolled, TRUE);
	gtk_widget_set_hexpand(scrolled, TRUE);
	gtk_box_append(GTK_BOX(vbox2), scrolled);

	p->service_list_view = gtk_list_view_new(GTK_SELECTION_MODEL(sel), factory);
	g_signal_connect(p->service_list_view, "activate", G_CALLBACK(service_activate_cb), d);
	g_signal_connect(sel, "notify::selected", G_CALLBACK(service_selection_changed_cb), d);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), p->service_list_view);

	p->service_progress_bar = gtk_progress_bar_new();
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(p->service_progress_bar), _("Browsing..."));
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(p->service_progress_bar), 0.1);
	gtk_box_append(GTK_BOX(vbox2), p->service_progress_bar);

	p->domain_button = gtk_button_new_with_mnemonic(_("_Domain..."));
	g_signal_connect(p->domain_button, "clicked", G_CALLBACK(domain_button_clicked), d);
	gtk_dialog_add_action_widget(GTK_DIALOG(d), p->domain_button, GTK_RESPONSE_NONE);

	gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT);
	gtk_window_set_default_size(GTK_WINDOW(d), 500, 450);

	p->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
	p->service_pulse_timeout = g_timeout_add(100, service_pulse_callback, d);
	p->start_idle = g_idle_add(start_callback, d);

	g_signal_connect(d, "response", G_CALLBACK(response_callback), d);
}

static void restart_browsing(AuiServiceDialog *d)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	if (p->start_idle == 0)
		p->start_idle = g_idle_add(start_callback, d);
}

void aui_service_dialog_set_browse_service_types(AuiServiceDialog *d, const gchar *type, ...)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	va_list ap;
	const char *t;
	unsigned u;

	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	g_return_if_fail(type);

	g_strfreev(p->browse_service_types);
	va_start(ap, type);
	for (u = 1; va_arg(ap, const char *); u++)
		;
	va_end(ap);
	p->browse_service_types = g_new0(gchar *, u + 1);
	p->browse_service_types[0] = g_strdup(type);
	va_start(ap, type);
	for (u = 1; (t = va_arg(ap, const char *)); u++)
		p->browse_service_types[u] = g_strdup(t);
	va_end(ap);

	if (p->browse_service_types[0] && p->browse_service_types[1])
		p->type_column_visible = TRUE;
	restart_browsing(d);
}

void aui_service_dialog_set_browse_service_typesv(AuiServiceDialog *d, const gchar *const *types)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);

	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	g_return_if_fail(types);
	g_return_if_fail(*types);

	g_strfreev(p->browse_service_types);
	p->browse_service_types = g_strdupv((char **)types);
	if (p->browse_service_types[0] && p->browse_service_types[1])
		p->type_column_visible = TRUE;
	restart_browsing(d);
}

const gchar *const *aui_service_dialog_get_browse_service_types(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	return (const gchar *const *)p->browse_service_types;
}

void aui_service_dialog_set_service_type_name(AuiServiceDialog *d, const gchar *type, const gchar *name)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	g_return_if_fail(type);
	g_return_if_fail(name);

	if (!p->service_type_names)
		p->service_type_names = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(p->service_type_names, g_strdup(type), g_strdup(name));

	{
		guint n, i;
		AuiServiceRow *row;
		n = g_list_model_get_n_items(G_LIST_MODEL(p->service_list_store));
		for (i = 0; i < n; i++) {
			row = g_list_model_get_item(G_LIST_MODEL(p->service_list_store), i);
			if (row && g_str_equal(row->type, type)) {
				g_free(row->pretty_type);
				row->pretty_type = g_strdup(name);
			}
			if (row)
				g_object_unref(row);
		}
	}
}

void aui_service_dialog_set_domain(AuiServiceDialog *d, const gchar *domain)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	g_return_if_fail(!domain || is_valid_domain_suffix(domain));

	g_free(p->domain);
	p->domain = domain ? avahi_normalize_name_strdup(domain) : NULL;
	restart_browsing(d);
}

const gchar *aui_service_dialog_get_domain(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	return p->domain;
}

void aui_service_dialog_set_service_type(AuiServiceDialog *d, const gchar *name)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	g_free(p->service_type);
	p->service_type = g_strdup(name);
}

const gchar *aui_service_dialog_get_service_type(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	return p->service_type;
}

void aui_service_dialog_set_service_name(AuiServiceDialog *d, const gchar *name)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	g_free(p->service_name);
	p->service_name = g_strdup(name);
}

const gchar *aui_service_dialog_get_service_name(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	return p->service_name;
}

const AvahiAddress *aui_service_dialog_get_address(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	g_return_val_if_fail(p->resolve_service_done && p->resolve_host_name_done, NULL);
	return &p->address;
}

guint16 aui_service_dialog_get_port(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), 0);
	g_return_val_if_fail(p->resolve_service_done, 0);
	return p->port;
}

const gchar *aui_service_dialog_get_host_name(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	g_return_val_if_fail(p->resolve_service_done, NULL);
	return p->host_name;
}

const AvahiStringList *aui_service_dialog_get_txt_data(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), NULL);
	g_return_val_if_fail(p->resolve_service_done, NULL);
	return p->txt_data;
}

void aui_service_dialog_set_resolve_service(AuiServiceDialog *d, gboolean resolve)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	p->resolve_service = resolve;
}

gboolean aui_service_dialog_get_resolve_service(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), FALSE);
	return p->resolve_service;
}

void aui_service_dialog_set_resolve_host_name(AuiServiceDialog *d, gboolean resolve)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	p->resolve_host_name = resolve;
}

gboolean aui_service_dialog_get_resolve_host_name(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), FALSE);
	return p->resolve_host_name;
}

void aui_service_dialog_set_address_family(AuiServiceDialog *d, AvahiProtocol proto)
{
	AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_if_fail(AUI_IS_SERVICE_DIALOG(d));
	p->address_family = proto;
}

AvahiProtocol aui_service_dialog_get_address_family(AuiServiceDialog *d)
{
	const AuiServiceDialogPrivate *p = aui_service_dialog_get_instance_private(d);
	g_return_val_if_fail(AUI_IS_SERVICE_DIALOG(d), AVAHI_PROTO_UNSPEC);
	return p->address_family;
}

static void aui_service_dialog_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
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

static void aui_service_dialog_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	AuiServiceDialog *d = AUI_SERVICE_DIALOG(object);
	switch (prop_id) {
	case PROP_BROWSE_SERVICE_TYPES:
		g_value_set_pointer(value, (gpointer)aui_service_dialog_get_browse_service_types(d));
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
		g_value_set_pointer(value, (gpointer)aui_service_dialog_get_address(d));
		break;
	case PROP_PORT:
		g_value_set_uint(value, aui_service_dialog_get_port(d));
		break;
	case PROP_HOST_NAME:
		g_value_set_string(value, aui_service_dialog_get_host_name(d));
		break;
	case PROP_TXT_DATA:
		g_value_set_pointer(value, (gpointer)aui_service_dialog_get_txt_data(d));
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

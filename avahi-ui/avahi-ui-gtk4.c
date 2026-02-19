/*
 * Stub GTK4 implementation of the avahi-ui API.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <string.h>

#include <gtk/gtk.h>

#include <avahi-common/address.h>
#include <avahi-common/domain.h>

#include "avahi-ui.h"

struct _AuiServiceDialogPrivate {
	int _unused;
};

G_DEFINE_TYPE_WITH_PRIVATE(AuiServiceDialog, aui_service_dialog, GTK_TYPE_DIALOG)

static void aui_service_dialog_class_init(AuiServiceDialogClass *klass)
{
	(void) klass;
}

static void aui_service_dialog_init(AuiServiceDialog *d)
{
	(void) d;
}

GtkWidget *aui_service_dialog_new_valist(
	const gchar *title,
	GtkWindow *parent,
	const gchar *first_button_text,
	va_list varargs)
{
	const gchar *button_text;
	GtkWidget *w;

	w = GTK_WIDGET(g_object_new(AUI_TYPE_SERVICE_DIALOG,
		"title", title,
		NULL));

	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(w), parent);

	button_text = first_button_text;
	while (button_text) {
		gint response_id = va_arg(varargs, gint);
		gtk_dialog_add_button(GTK_DIALOG(w), button_text, response_id);
		button_text = va_arg(varargs, const gchar *);
	}

	return w;
}

GtkWidget *aui_service_dialog_new(
	const gchar *title,
	GtkWindow *parent,
	const gchar *first_button_text,
	...)
{
	GtkWidget *w;
	va_list varargs;

	va_start(varargs, first_button_text);
	w = aui_service_dialog_new_valist(title, parent, first_button_text, varargs);
	va_end(varargs);

	return w;
}

void aui_service_dialog_set_browse_service_types(AuiServiceDialog *d, const gchar *type, ...)
{
	(void) d;
	(void) type;
}

void aui_service_dialog_set_browse_service_typesv(AuiServiceDialog *d, const gchar *const *types)
{
	(void) d;
	(void) types;
}

const gchar *const *aui_service_dialog_get_browse_service_types(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

void aui_service_dialog_set_service_type_name(AuiServiceDialog *d, const gchar *type, const gchar *name)
{
	(void) d;
	(void) type;
	(void) name;
}

void aui_service_dialog_set_domain(AuiServiceDialog *d, const gchar *domain)
{
	(void) d;
	(void) domain;
}

const gchar *aui_service_dialog_get_domain(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

void aui_service_dialog_set_service_type(AuiServiceDialog *d, const gchar *name)
{
	(void) d;
	(void) name;
}

const gchar *aui_service_dialog_get_service_type(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

void aui_service_dialog_set_service_name(AuiServiceDialog *d, const gchar *name)
{
	(void) d;
	(void) name;
}

const gchar *aui_service_dialog_get_service_name(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

const AvahiAddress *aui_service_dialog_get_address(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

guint16 aui_service_dialog_get_port(AuiServiceDialog *d)
{
	(void) d;
	return 0;
}

const gchar *aui_service_dialog_get_host_name(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

const AvahiStringList *aui_service_dialog_get_txt_data(AuiServiceDialog *d)
{
	(void) d;
	return NULL;
}

void aui_service_dialog_set_resolve_service(AuiServiceDialog *d, gboolean resolve)
{
	(void) d;
	(void) resolve;
}

gboolean aui_service_dialog_get_resolve_service(AuiServiceDialog *d)
{
	(void) d;
	return TRUE;
}

void aui_service_dialog_set_resolve_host_name(AuiServiceDialog *d, gboolean resolve)
{
	(void) d;
	(void) resolve;
}

gboolean aui_service_dialog_get_resolve_host_name(AuiServiceDialog *d)
{
	(void) d;
	return TRUE;
}

void aui_service_dialog_set_address_family(AuiServiceDialog *d, AvahiProtocol proto)
{
	(void) d;
	(void) proto;
}

AvahiProtocol aui_service_dialog_get_address_family(AuiServiceDialog *d)
{
	(void) d;
	return AVAHI_PROTO_UNSPEC;
}

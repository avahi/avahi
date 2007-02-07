#ifndef fooavahiuihfoo
#define fooavahiuihfoo

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

#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>

#include <avahi-client/client.h>

G_BEGIN_DECLS

#define AUI_TYPE_SERVICE_DIALOG            (aui_service_dialog_get_type())
#define AUI_SERVICE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), AUI_TYPE_SERVICE_DIALOG, AuiServiceDialog))
#define AUI_SERVICE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), AUI_TYPE_SERVICE_DIALOG, AuiServiceDialogClass))
#define AUI_IS_SERVICE_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), AUI_TYPE_SERVICE_DIALOG))
#define AUI_IS_SERVICE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), AUI_TYPE_SERVICE_DIALOG))
#define AUI_SERVICE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), AUI_TYPE_SERVICE_DIALOG, AuiServiceDialogClass))

typedef struct _AuiServiceDialog AuiServiceDialog;
typedef struct _AuiServiceDialogClass  AuiServiceDialogClass;

struct _AuiServiceDialogClass {
    GtkDialogClass parent_class;
    
    /* Padding for future expansion */
    void (*_aui_reserved1)(void);
    void (*_aui_reserved2)(void);
    void (*_aui_reserved3)(void);
    void (*_aui_reserved4)(void);
};

/* ServiceDialog */
GType aui_service_dialog_get_type(void) G_GNUC_CONST;
GtkWidget* aui_service_dialog_new(const gchar *title);

void aui_service_dialog_set_browse_service_types(AuiServiceDialog *d, const gchar *type, ...) G_GNUC_NULL_TERMINATED;
void aui_service_dialog_set_browse_service_typesv(AuiServiceDialog *d, const gchar *const*type);
const gchar*const* aui_service_dialog_get_browse_service_types(AuiServiceDialog *d);

void aui_service_dialog_set_domain(AuiServiceDialog *d, const gchar *domain);
const gchar* aui_service_dialog_get_domain(AuiServiceDialog *d);

void aui_service_dialog_set_service_type(AuiServiceDialog *d, const gchar *name);
const gchar* aui_service_dialog_get_service_type(AuiServiceDialog *d);

void aui_service_dialog_set_service_name(AuiServiceDialog *d, const gchar *name);
const gchar* aui_service_dialog_get_service_name(AuiServiceDialog *d);

const AvahiAddress* aui_service_dialog_get_address(AuiServiceDialog *d);
guint16 aui_service_dialog_get_port(AuiServiceDialog *d);
const gchar* aui_service_dialog_get_host_name(AuiServiceDialog *d);
const AvahiStringList *aui_service_dialog_get_txt_data(AuiServiceDialog *d);

void aui_service_dialog_set_resolve_service(AuiServiceDialog *d, gboolean resolve);
gboolean aui_service_dialog_get_resolve_service(AuiServiceDialog *d);

void aui_service_dialog_set_resolve_host_name(AuiServiceDialog *d, gboolean resolve);
gboolean aui_service_dialog_get_resolve_host_name(AuiServiceDialog *d);

void aui_service_dialog_set_address_family(AuiServiceDialog *d, AvahiProtocol proto);
AvahiProtocol aui_service_dialog_get_address_family(AuiServiceDialog *d);

G_END_DECLS

#endif



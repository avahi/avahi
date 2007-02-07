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
#include <unistd.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <avahi-client/client.h>
#include <avahi-common/strlst.h>
#include <avahi-common/malloc.h>

#include "avahi-ui.h"

int main(int argc, char*argv[]) {
    GtkWidget *d;

    gtk_init(&argc, &argv);
    
    d = aui_service_dialog_new("Choose SSH server");
    aui_service_dialog_set_browse_service_types(AUI_SERVICE_DIALOG(d), "_ssh._tcp",  /*"_ftp._tcp", "_http._tcp",*/ NULL);
    aui_service_dialog_set_resolve_service(AUI_SERVICE_DIALOG(d), TRUE);
    aui_service_dialog_set_resolve_host_name(AUI_SERVICE_DIALOG(d), !avahi_nss_support());

    gtk_window_present(GTK_WINDOW(d));

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        char p[16], a[AVAHI_ADDRESS_STR_MAX], *u = NULL;
        char *h = NULL;
        const AvahiStringList *txt;
        
        snprintf(p, sizeof(p), "%u", aui_service_dialog_get_port(AUI_SERVICE_DIALOG(d)));

        if (avahi_nss_support())
            h = g_strdup(aui_service_dialog_get_host_name(AUI_SERVICE_DIALOG(d)));
        else
            h = g_strdup(avahi_address_snprint(a, sizeof(a), aui_service_dialog_get_address(AUI_SERVICE_DIALOG(d))));

        for (txt = aui_service_dialog_get_txt_data(AUI_SERVICE_DIALOG(d)); txt; txt = txt->next) {
            char *key, *value;
            
            if (avahi_string_list_get_pair((AvahiStringList*) txt, &key, &value, NULL) < 0)
                break;

            if (strcmp(key, "u") == 0)
                u = g_strdup(value);
            
            avahi_free(key);
            avahi_free(value);
        }

        gtk_widget_destroy(d);

        if (u) {
            g_print("ssh -p %s -l %s %s\n", p, u, h);
            execlp("ssh", "ssh", "-p", p, "-l", u, h, NULL); 
        } else {
            g_print("ssh -p %s %s\n", p, h);
            execlp("ssh", "ssh", "-p", p, h, NULL); 
        }

        g_warning("execlp() failed: %s", strerror(errno));

        g_free(h);
        g_free(u);
        
    } else {
        gtk_widget_destroy(d);

        g_print("Canceled.\n");
    }
    
    return 1;
    
}

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dbus/dbus.h>

#include <avahi-client/client.h>
#include <avahi-common/dbus.h>
#include <avahi-common/llist.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>

#include "client.h"
#include "internal.h"

static int simple_method_call(AvahiClient *client, const char *path, const char *interface, const char *method) {
    DBusMessage *message = NULL, *reply = NULL;
    DBusError error;
    int r = AVAHI_OK;
    
    dbus_error_init(&error);

    assert(client);
    assert(path);
    assert(interface);
    assert(method);
    
    if (!(message = dbus_message_new_method_call(AVAHI_DBUS_NAME, path, interface, method))) {
        r = avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }
        
    if (!(reply = dbus_connection_send_with_reply_and_block(client->bus, message, -1, &error)) ||
        dbus_error_is_set (&error)) {
        r = avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }
    
    if (!dbus_message_get_args(reply, &error, DBUS_TYPE_INVALID) ||
        dbus_error_is_set (&error)) {
        r = avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);

    return AVAHI_OK;
    
fail:
    if (dbus_error_is_set(&error)) {
        r = avahi_client_set_dbus_error(client, &error);
        dbus_error_free(&error);
    }

    if (message)
        dbus_message_unref(message);

    if (reply)
        dbus_message_unref(reply);

    return r;
}

AvahiDomainBrowser* avahi_domain_browser_new(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiDomainBrowserType btype,
    AvahiDomainBrowserCallback callback,
    void *userdata) {
    
    AvahiDomainBrowser *db = NULL;
    DBusMessage *message = NULL, *reply;
    DBusError error;
    char *path;
    int32_t i_interface, i_protocol, bt;

    assert(client);
    assert(callback);

    dbus_error_init (&error);

    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        goto fail;
    }

    if (!domain)
        domain = "";

    if (!(db = avahi_new (AvahiDomainBrowser, 1))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    db->client = client;
    db->callback = callback;
    db->userdata = userdata;
    db->path = NULL;

    AVAHI_LLIST_PREPEND(AvahiDomainBrowser, domain_browsers, client->domain_browsers, db);

    if (!(message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, "DomainBrowserNew"))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    i_interface = interface;
    i_protocol = protocol;
    bt = btype;

    if (!(dbus_message_append_args(
              message,
              DBUS_TYPE_INT32, &i_interface,
              DBUS_TYPE_INT32, &i_protocol,
              DBUS_TYPE_STRING, &domain,
              DBUS_TYPE_INT32, &bt,
              DBUS_TYPE_INVALID))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    if (!(reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error)) ||
        dbus_error_is_set(&error)) {
        avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (!dbus_message_get_args (reply, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID) ||
        dbus_error_is_set(&error) ||
        !path) {
        avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (!(db->path = avahi_strdup(path))) {

        /* FIXME: We don't remove the object on the server side */

        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);
    
    return db;

fail:

    if (dbus_error_is_set(&error)) {
        avahi_client_set_dbus_error(client, &error);
        dbus_error_free(&error);
    }

    if (db)
        avahi_domain_browser_free(db);
    
    if (message)
        dbus_message_unref(message);

    if (reply)
        dbus_message_unref(reply);

    return NULL;
}

int avahi_domain_browser_free (AvahiDomainBrowser *b) {
    AvahiClient *client;
    int r = AVAHI_OK;

    assert(b);
    client = b->client;

    if (b->path && client->state != AVAHI_CLIENT_DISCONNECTED)
        r = simple_method_call(client, b->path, AVAHI_DBUS_INTERFACE_DOMAIN_BROWSER, "Free");

    AVAHI_LLIST_REMOVE(AvahiDomainBrowser, domain_browsers, client->domain_browsers, b);

    avahi_free(b->path);
    avahi_free(b);

    return r;
}

const char* avahi_domain_browser_get_dbus_path(AvahiDomainBrowser *b) {
    assert(b);
    
    return b->path;
}

DBusHandlerResult avahi_domain_browser_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message) {
    AvahiDomainBrowser *db = NULL;
    DBusError error;
    const char *path;
    char *domain;
    int32_t interface, protocol;

    assert(client);
    assert(message);
    
    dbus_error_init (&error);

    if (!(path = dbus_message_get_path(message)))
        goto fail;

    for (db = client->domain_browsers; db; db = db->domain_browsers_next)
        if (strcmp (db->path, path) == 0)
            break;

    if (!db)
        goto fail;

    if (!dbus_message_get_args(
              message, &error,
              DBUS_TYPE_INT32, &interface,
              DBUS_TYPE_INT32, &protocol,
              DBUS_TYPE_STRING, &domain,
              DBUS_TYPE_INVALID) ||
          dbus_error_is_set (&error)) {
        fprintf(stderr, "Failed to parse browser event.\n");
        goto fail;
    }

    db->callback(db, (AvahiIfIndex) interface, (AvahiProtocol) protocol, event, domain, db->userdata);

    return DBUS_HANDLER_RESULT_HANDLED;

fail:
    dbus_error_free (&error);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* AvahiServiceTypeBrowser */
AvahiServiceTypeBrowser* avahi_service_type_browser_new(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *domain,
    AvahiServiceTypeBrowserCallback callback,
    void *userdata) {
        
    AvahiServiceTypeBrowser *b = NULL;
    DBusMessage *message = NULL, *reply;
    DBusError error;
    char *path;
    int32_t i_interface, i_protocol;

    assert(client);
    assert(callback);

    dbus_error_init(&error);

    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        goto fail;
    }

    if (!domain)
        domain = "";

    if (!(b = avahi_new(AvahiServiceTypeBrowser, 1))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    b->client = client;
    b->callback = callback;
    b->userdata = userdata;
    b->path = NULL;

    AVAHI_LLIST_PREPEND(AvahiServiceTypeBrowser, service_type_browsers, client->service_type_browsers, b);

    if (!(message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, "ServiceTypeBrowserNew"))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }
    
    i_interface = interface;
    i_protocol = protocol;

    if (!dbus_message_append_args(
            message,
            DBUS_TYPE_INT32, &interface,
            DBUS_TYPE_INT32, &protocol,
            DBUS_TYPE_STRING, &domain,
            DBUS_TYPE_INVALID)) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    if (!(reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error)) ||
        dbus_error_is_set(&error)) {
        avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (!dbus_message_get_args (reply, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID) ||
        dbus_error_is_set(&error) ||
        !path) {
        avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (!(b->path = avahi_strdup(path))) {

        /* FIXME: We don't remove the object on the server side */

        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);

    return b;

fail:
    
    if (dbus_error_is_set(&error)) {
        avahi_client_set_dbus_error(client, &error);
        dbus_error_free(&error);
    }

    if (b)
        avahi_service_type_browser_free(b);
    
    if (message)
        dbus_message_unref(message);

    if (reply)
        dbus_message_unref(reply);

    return NULL;
}

int avahi_service_type_browser_free (AvahiServiceTypeBrowser *b) {
    AvahiClient *client;
    int r = AVAHI_OK;

    assert(b);
    client = b->client;

    if (b->path && client->state != AVAHI_CLIENT_DISCONNECTED)
        r = simple_method_call(client, b->path, AVAHI_DBUS_INTERFACE_SERVICE_TYPE_BROWSER, "Free");

    AVAHI_LLIST_REMOVE(AvahiServiceTypeBrowser, service_type_browsers, b->client->service_type_browsers, b);

    avahi_free(b->path);
    avahi_free(b);
    return r;
}

const char* avahi_service_type_browser_get_dbus_path(AvahiServiceTypeBrowser *b) {
    assert(b);
    
    return b->path;
}

DBusHandlerResult avahi_service_type_browser_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message) {
    AvahiServiceTypeBrowser *b = NULL;
    DBusError error;
    const char *path;
    char *domain, *type;
    int32_t interface, protocol;

    assert(client);
    assert(message);
    
    dbus_error_init (&error);

    if (!(path = dbus_message_get_path(message)))
        goto fail;

    for (b = client->service_type_browsers; b; b = b->service_type_browsers_next)
        if (strcmp (b->path, path) == 0)
            break;

    if (!b)
        goto fail;

    if (!dbus_message_get_args(
              message, &error,
              DBUS_TYPE_INT32, &interface,
              DBUS_TYPE_INT32, &protocol,
              DBUS_TYPE_STRING, &type,
              DBUS_TYPE_STRING, &domain,
              DBUS_TYPE_INVALID) ||
          dbus_error_is_set(&error)) {
        fprintf(stderr, "Failed to parse browser event.\n");
        goto fail;
    }

    b->callback(b, (AvahiIfIndex) interface, (AvahiProtocol) protocol, event, type, domain, b->userdata);

    return DBUS_HANDLER_RESULT_HANDLED;

fail:
    dbus_error_free (&error);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* AvahiServiceBrowser */

AvahiServiceBrowser* avahi_service_browser_new(
    AvahiClient *client,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    const char *type,
    const char *domain,
    AvahiServiceBrowserCallback callback,
    void *userdata) {
    
    AvahiServiceBrowser *b = NULL;
    DBusMessage *message = NULL, *reply;
    DBusError error;
    char *path;
    int32_t i_protocol, i_interface;

    assert(client);
    assert(type);
    assert(callback);

    dbus_error_init(&error);

    if (client->state == AVAHI_CLIENT_DISCONNECTED) {
        avahi_client_set_errno(client, AVAHI_ERR_BAD_STATE);
        goto fail;
    }

    if (!domain)
        domain = "";

    if (!(b = avahi_new(AvahiServiceBrowser, 1))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }
    
    b->client = client;
    b->callback = callback;
    b->userdata = userdata;
    b->path = NULL;

    AVAHI_LLIST_PREPEND(AvahiServiceBrowser, service_browsers, client->service_browsers, b);

    if (!(message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER, AVAHI_DBUS_INTERFACE_SERVER, "ServiceBrowserNew"))) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    i_interface = interface;
    i_protocol = protocol;
    

    if (!dbus_message_append_args(
            message,
            DBUS_TYPE_INT32, &i_interface,
            DBUS_TYPE_INT32, &i_protocol,
            DBUS_TYPE_STRING, &type,
            DBUS_TYPE_STRING, &domain,
            DBUS_TYPE_INVALID)) {
        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    if (!(reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error)) ||
        dbus_error_is_set(&error)) {
        avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (!dbus_message_get_args (reply, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID) ||
        dbus_error_is_set(&error) ||
        !path) {
        avahi_client_set_errno(client, AVAHI_ERR_DBUS_ERROR);
        goto fail;
    }

    if (!(b->path = avahi_strdup(path))) {

        /* FIXME: We don't remove the object on the server side */

        avahi_client_set_errno(client, AVAHI_ERR_NO_MEMORY);
        goto fail;
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);
    
    return b;

fail:
    if (dbus_error_is_set(&error)) {
        avahi_client_set_dbus_error(client, &error);
        dbus_error_free(&error);
    }

    if (b)
        avahi_service_browser_free(b);
    
    if (message)
        dbus_message_unref(message);

    if (reply)
        dbus_message_unref(reply);

    return NULL;
}
    
int avahi_service_browser_free (AvahiServiceBrowser *b) {
    AvahiClient *client;
    int r = AVAHI_OK;

    assert(b);
    client = b->client;

    if (b->path && client->state != AVAHI_CLIENT_DISCONNECTED)
        r = simple_method_call(client, b->path, AVAHI_DBUS_INTERFACE_SERVICE_BROWSER, "Free");

    AVAHI_LLIST_REMOVE(AvahiServiceBrowser, service_browsers, b->client->service_browsers, b);

    avahi_free(b->path);
    avahi_free(b);
    return r;
}

const char* avahi_service_browser_get_dbus_path(AvahiServiceBrowser *b) {
    assert(b);
    
    return b->path;
}

DBusHandlerResult avahi_service_browser_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message) {
    AvahiServiceBrowser *b = NULL;
    DBusError error;
    const char *path;
    char *name, *type, *domain;
    int32_t interface, protocol;

    dbus_error_init (&error);

    if (!(path = dbus_message_get_path(message)))
        goto fail;

    for (b = client->service_browsers; b; b = b->service_browsers_next)
        if (strcmp (b->path, path) == 0)
            break;

    if (!b)
        goto fail;

    if (!dbus_message_get_args (
              message, &error,
              DBUS_TYPE_INT32, &interface,
              DBUS_TYPE_INT32, &protocol,
              DBUS_TYPE_STRING, &name,
              DBUS_TYPE_STRING, &type,
              DBUS_TYPE_STRING, &domain,
              DBUS_TYPE_INVALID) ||
          dbus_error_is_set(&error)) {
        fprintf(stderr, "Failed to parse browser event.\n");
        goto fail;
    }

    b->callback(b, (AvahiIfIndex) interface, (AvahiProtocol) protocol, event, name, type, domain, b->userdata);

    return DBUS_HANDLER_RESULT_HANDLED;

fail:
    dbus_error_free (&error);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}



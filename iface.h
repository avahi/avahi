#ifndef fooifacehfoo
#define fooifacehfoo

#include <glib.h>

#include "address.h"

struct _flxInterfaceMonitor;
typedef struct _flxInterfaceMonitor flxInterfaceMonitor;

struct _flxInterfaceAddress;
typedef struct _flxInterfaceAddress flxInterfaceAddress;

struct _flxInterface;
typedef struct _flxInterface flxInterface;

struct _flxInterface {
    gchar *name;
    gint index;
    guint flags;

    flxInterfaceAddress *addresses;
    flxInterface *next, *prev;
};

struct _flxInterfaceAddress {
    guchar flags;
    guchar scope;
    flxAddress address;
    
    flxInterface *interface;
    flxInterfaceAddress *next, *prev;
};

typedef enum { FLX_INTERFACE_NEW, FLX_INTERFACE_REMOVE, FLX_INTERFACE_CHANGE } flxInterfaceChange;

flxInterfaceMonitor *flx_interface_monitor_new(GMainContext *c);
void flx_interface_monitor_free(flxInterfaceMonitor *m);

const flxInterface* flx_interface_monitor_get_interface(flxInterfaceMonitor *m, gint index);
const flxInterface* flx_interface_monitor_get_first(flxInterfaceMonitor *m);

void flx_interface_monitor_add_interface_callback(
    flxInterfaceMonitor *m,
    void (*cb)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterface *i, gpointer userdata),
    gpointer userdata);

void flx_interface_monitor_remove_interface_callback(
    flxInterfaceMonitor *m,
    void (*cb)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterface *i, gpointer userdata),
    gpointer userdata);

void flx_interface_monitor_add_address_callback(
    flxInterfaceMonitor *m,
    void (*cb)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *a, gpointer userdata),
    gpointer userdata);

void flx_interface_monitor_remove_address_callback(
    flxInterfaceMonitor *m,
    void (*cb)(flxInterfaceMonitor *m, flxInterfaceChange change, const flxInterfaceAddress *a, gpointer userdata),
    gpointer userdata);

#endif

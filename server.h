#ifndef fooflxserverhfoo
#define fooflxserverhfoo

#include "flx.h"
#include "iface.h"

struct _flxEntry {
    flxRecord rr;
    gint id;
    gint interface;

    int unique;

    struct _flxEntry *next, *prev;
    struct _flxEntry *next_by_name, *prev_by_name;
    struct _flxEntry *next_by_id, *prev_by_id;
};

typedef struct _flxEntry flxEntry;

struct _flxServer {
    GMainContext *context;
    flxInterfaceMonitor *monitor;

    gint current_id;
    
    GHashTable *rrset_by_id;
    GHashTable *rrset_by_name;

    flxEntry *entries;
};

#endif

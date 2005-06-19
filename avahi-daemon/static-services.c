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

#include <sys/stat.h>
#include <glob.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <expat.h>

#include <avahi-core/llist.h>
#include <avahi-core/log.h>

#include "main.h"

typedef struct StaticService StaticService;
typedef struct StaticServiceGroup StaticServiceGroup;

struct StaticService {
    StaticServiceGroup *group;
    
    gchar *type;
    gchar *domain_name;
    gchar *host_name;
    guint16 port;

    AvahiStringList *txt_records;
    
    AVAHI_LLIST_FIELDS(StaticService, services);
};

struct StaticServiceGroup {
    gchar *filename;
    time_t mtime;

    gchar *name, *chosen_name;
    gboolean replace_wildcards;

    AvahiEntryGroup *entry_group;
    AVAHI_LLIST_HEAD(StaticService, services);
    AVAHI_LLIST_FIELDS(StaticServiceGroup, groups);
};

static AVAHI_LLIST_HEAD(StaticServiceGroup, groups) = NULL;

static gchar *replacestr(const gchar *pattern, const gchar *a, const gchar *b) {
    gchar *r = NULL, *e, *n;

    while ((e = strstr(pattern, a))) {
        gchar *k;

        k = g_strndup(pattern, e - pattern);
        if (r)
            n = g_strconcat(r, k, b, NULL);
        else
            n = g_strconcat(k, b, NULL);

        g_free(k);
        g_free(r);
        r = n;

        pattern = e + strlen(a);
    }

    if (!r)
        return g_strdup(pattern);

    n = g_strconcat(r, pattern, NULL);
    g_free(r);

    return n;
}

static void add_static_service_group_to_server(StaticServiceGroup *g);
static void remove_static_service_group_from_server(StaticServiceGroup *g);

static StaticService *static_service_new(StaticServiceGroup *group) {
    StaticService *s;
    
    g_assert(group);
    s = g_new(StaticService, 1);
    s->group = group;

    s->type = s->host_name = s->domain_name = NULL;
    s->port = 0;

    s->txt_records = NULL;

    AVAHI_LLIST_PREPEND(StaticService, services, group->services, s);

    return s;
}

static StaticServiceGroup *static_service_group_new(gchar *filename) {
    StaticServiceGroup *g;
    g_assert(filename);

    g = g_new(StaticServiceGroup, 1);
    g->filename = g_strdup(filename);
    g->mtime = 0;
    g->name = g->chosen_name = NULL;
    g->replace_wildcards = FALSE;
    g->entry_group = NULL;

    AVAHI_LLIST_HEAD_INIT(StaticService, g->services);
    AVAHI_LLIST_PREPEND(StaticServiceGroup, groups, groups, g);

    return g;
}

static void static_service_free(StaticService *s) {
    g_assert(s);
    
    AVAHI_LLIST_REMOVE(StaticService, services, s->group->services, s);

    g_free(s->type);
    g_free(s->host_name);
    g_free(s->domain_name);

    avahi_string_list_free(s->txt_records);
    
    g_free(s);
}

static void static_service_group_free(StaticServiceGroup *g) {
    g_assert(g);

    remove_static_service_group_from_server(g);

    while (g->services)
        static_service_free(g->services);

    AVAHI_LLIST_REMOVE(StaticServiceGroup, groups, groups, g);

    g_free(g->filename);
    g_free(g->name);
    g_free(g->chosen_name);
    g_free(g);
}

static void entry_group_callback(AvahiServer *s, AvahiEntryGroup *eg, AvahiEntryGroupState state, gpointer userdata) {
    StaticServiceGroup *g = userdata;
    
    g_assert(s);
    g_assert(g);
    
    if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        gchar *n;

        remove_static_service_group_from_server(g);

        n = avahi_alternative_service_name(g->chosen_name);
        g_free(g->chosen_name);
        g->chosen_name = n;

        avahi_log_notice("Service name conflict for \"%s\" (%s), retrying with \"%s\".", g->name, g->filename, g->chosen_name);

        add_static_service_group_to_server(g);
    } else if (state == AVAHI_ENTRY_GROUP_ESTABLISHED)
        avahi_log_info("Service \"%s\" (%s) successfully established.", g->chosen_name, g->filename);
}

static void add_static_service_group_to_server(StaticServiceGroup *g) {
    StaticService *s;

    g_assert(g);
    
    if (g->entry_group)
        return;
    
    if (g->chosen_name)
        g_free(g->chosen_name);
    
    if (g->replace_wildcards)
        g->chosen_name = replacestr(g->name, "%h", avahi_server_get_host_name(avahi_server));
    else
        g->chosen_name = g_strdup(g->name);
    
    g->entry_group = avahi_entry_group_new(avahi_server, entry_group_callback, g);
    
    for (s = g->services; s; s = s->services_next) {

        if (avahi_server_add_service_strlst(
                avahi_server,
                g->entry_group,
                -1, AF_UNSPEC,
                s->type, g->chosen_name,
                s->domain_name, s->host_name, s->port,
                avahi_string_list_copy(s->txt_records)) < 0) {
            avahi_log_error("Failed to add service '%s' of type '%s', ignoring service group (%s)", g->chosen_name, s->type, g->filename);
            remove_static_service_group_from_server(g);
            return;
        }
    }

    avahi_entry_group_commit(g->entry_group);
}

static void remove_static_service_group_from_server(StaticServiceGroup *g) {
    g_assert(g);

    if (g->entry_group) {
        avahi_entry_group_free(g->entry_group);
        g->entry_group = NULL;
    }
}

typedef enum {
    XML_TAG_INVALID,
    XML_TAG_SERVICE_GROUP,
    XML_TAG_NAME,
    XML_TAG_SERVICE,
    XML_TAG_TYPE,
    XML_TAG_DOMAIN_NAME,
    XML_TAG_HOST_NAME,
    XML_TAG_PORT,
    XML_TAG_TXT_RECORD
} xml_tag_name;

struct xml_userdata {
    StaticServiceGroup *group;
    StaticService *service;
    xml_tag_name current_tag;
    gboolean failed;
    gchar *buf;
};

static void XMLCALL xml_start(void *data, const char *el, const char *attr[]) {
    struct xml_userdata *u = data;
    
    g_assert(u);

    if (u->failed)
        return;

    if (u->current_tag == XML_TAG_INVALID && strcmp(el, "service-group") == 0) {

        if (attr[0])
            goto invalid_attr;

        u->current_tag = XML_TAG_SERVICE_GROUP;
    } else if (u->current_tag == XML_TAG_SERVICE_GROUP && strcmp(el, "name") == 0) {
        u->current_tag = XML_TAG_NAME;

        if (attr[0]) {
            if (strcmp(attr[0], "replace-wildcards") == 0) 
                u->group->replace_wildcards = strcmp(attr[1], "yes") == 0;
            else
                goto invalid_attr;
        }

        if (attr[2])
            goto invalid_attr;
        
            
        
    } else if (u->current_tag == XML_TAG_SERVICE_GROUP && strcmp(el, "service") == 0) {
        if (attr[0])
            goto invalid_attr;

        g_assert(!u->service);
        u->service = static_service_new(u->group);

        u->current_tag = XML_TAG_SERVICE;
    } else if (u->current_tag == XML_TAG_SERVICE && strcmp(el, "type") == 0) {
        if (attr[0])
            goto invalid_attr;

        u->current_tag = XML_TAG_TYPE;
    } else if (u->current_tag == XML_TAG_SERVICE && strcmp(el, "domain-name") == 0) {
        if (attr[0])
            goto invalid_attr;
        
        u->current_tag = XML_TAG_DOMAIN_NAME;
    } else if (u->current_tag == XML_TAG_SERVICE && strcmp(el, "host-name") == 0) {
        if (attr[0])
            goto invalid_attr;
        
        u->current_tag = XML_TAG_HOST_NAME;
    } else if (u->current_tag == XML_TAG_SERVICE && strcmp(el, "port") == 0) {
        if (attr[0])
            goto invalid_attr;
        
        u->current_tag = XML_TAG_PORT;
    } else if (u->current_tag == XML_TAG_SERVICE && strcmp(el, "txt-record") == 0) {
        if (attr[0])
            goto invalid_attr;
        
        u->current_tag = XML_TAG_TXT_RECORD;
    } else {
        avahi_log_error("%s: parse failure: didn't expect element <%s>.", u->group->filename, el);
        u->failed = TRUE;
    }

    return;

invalid_attr:
    avahi_log_error("%s: parse failure: invalid attribute for element <%s>.", u->group->filename, el);
    u->failed = TRUE;
    return;
}
    
static void XMLCALL xml_end(void *data, const char *el) {
    struct xml_userdata *u = data;
    g_assert(u);

    if (u->failed)
        return;
    
    switch (u->current_tag) {
        case XML_TAG_SERVICE_GROUP:

            if (!u->group->name || !u->group->services) {
                avahi_log_error("%s: parse failure: service group incomplete.", u->group->filename);
                u->failed = TRUE;
                return;
            }
            
            u->current_tag = XML_TAG_INVALID;
            break;

        case XML_TAG_SERVICE:

            if (u->service->port == 0 || !u->service->type) {
                avahi_log_error("%s: parse failure: service incomplete.", u->group->filename);
                u->failed = TRUE;
                return;
            }
            
            u->service = NULL;
            u->current_tag = XML_TAG_SERVICE_GROUP;
            break;

        case XML_TAG_NAME:
            u->current_tag = XML_TAG_SERVICE_GROUP;
            break;

        case XML_TAG_PORT: {
            int p;
            g_assert(u->service);
            
            p = u->buf ? atoi(u->buf) : 0;

            if (p <= 0 || p > 0xFFFF) {
                avahi_log_error("%s: parse failure: invalid port specification \"%s\".", u->group->filename, u->buf);
                u->failed = TRUE;
                return;
            }

            u->service->port = (guint16) p;

            u->current_tag = XML_TAG_SERVICE;
            break;
        }

        case XML_TAG_TXT_RECORD: {
            g_assert(u->service);
            
            u->service->txt_records = avahi_string_list_add(u->service->txt_records, u->buf ? u->buf : "");
            u->current_tag = XML_TAG_SERVICE;
            break;
        }
            
        case XML_TAG_TYPE:
        case XML_TAG_DOMAIN_NAME:
        case XML_TAG_HOST_NAME:
            u->current_tag = XML_TAG_SERVICE;
            break;

        case XML_TAG_INVALID:
            ;
    }

    g_free(u->buf);
    u->buf = NULL;
}

static gchar *append_cdata(gchar *t, const gchar *n, int length) {
    gchar *r, *k;
    
    if (!length)
        return t;

    k = g_strndup(n, length);

    if (t) {
        r = g_strconcat(t, k, NULL);
        g_free(k);
        g_free(t);
    } else
        r = k;
    
    return r;
}

static void XMLCALL xml_cdata(void *data, const XML_Char *s, int len) {
    struct xml_userdata *u = data;
    g_assert(u);

    if (u->failed)
        return;

    switch (u->current_tag) {
        case XML_TAG_NAME:
            u->group->name = append_cdata(u->group->name, s, len);
            break;
            
        case XML_TAG_TYPE:
            g_assert(u->service);
            u->service->type = append_cdata(u->service->type, s, len);
            break;

        case XML_TAG_DOMAIN_NAME:
            g_assert(u->service);
            u->service->domain_name = append_cdata(u->service->domain_name, s, len);
            break;

        case XML_TAG_HOST_NAME:
            g_assert(u->service);
            u->service->host_name = append_cdata(u->service->host_name, s, len);
            break;

        case XML_TAG_PORT:
        case XML_TAG_TXT_RECORD:
            u->buf = append_cdata(u->buf, s, len);
            break;

        case XML_TAG_SERVICE_GROUP:
        case XML_TAG_SERVICE:
        case XML_TAG_INVALID:
            ;
    }
}

static gint static_service_group_load(StaticServiceGroup *g) {
    XML_Parser parser = NULL;
    gint fd = -1;
    struct xml_userdata u;
    gint r = -1;
    struct stat st;
    ssize_t n;

    g_assert(g);

    u.buf = NULL;
    u.group = g;
    u.service = NULL;
    u.current_tag = XML_TAG_INVALID;
    u.failed = FALSE;

    /* Cleanup old data in this service group, if available */
    remove_static_service_group_from_server(g);
    while (g->services)
        static_service_free(g->services);

    g_free(g->name);
    g_free(g->chosen_name);
    g->name = g->chosen_name = NULL;
    g->replace_wildcards = FALSE;
    
    if (!(parser = XML_ParserCreate(NULL))) {
        avahi_log_error("XML_ParserCreate() failed.");
        goto finish;
    }

    if ((fd = open(g->filename, O_RDONLY)) < 0) {
        avahi_log_error("open(\"%s\", O_RDONLY): %s", g->filename, strerror(errno));
        goto finish;
    }

    if (fstat(fd, &st) < 0) {
        avahi_log_error("fstat(): %s", strerror(errno));
        goto finish;
    }

    g->mtime = st.st_mtime;
    
    XML_SetUserData(parser, &u);

    XML_SetElementHandler(parser, xml_start, xml_end);
    XML_SetCharacterDataHandler(parser, xml_cdata);

    do {
        void *buffer;

#define BUFSIZE (10*1024)

        if (!(buffer = XML_GetBuffer(parser, BUFSIZE))) {
            avahi_log_error("XML_GetBuffer() failed.");
            goto finish;
        }

        if ((n = read(fd, buffer, BUFSIZE)) < 0) {
            avahi_log_error("read(): %s\n", strerror(errno));
            goto finish;
        }

        if (!XML_ParseBuffer(parser, n, n == 0)) {
            avahi_log_error("XML_ParseBuffer() failed at line %d: %s.\n", XML_GetCurrentLineNumber(parser), XML_ErrorString(XML_GetErrorCode(parser)));
            goto finish;
        }

    } while (n != 0);

    if (!u.failed)
        r = 0;

finish:

    if (fd >= 0)
        close(fd);

    if (parser)
        XML_ParserFree(parser);

    g_free(u.buf);

    return r;
}

static void load_file(gchar *n) {
    StaticServiceGroup *g;
    g_assert(n);

    for (g = groups; g; g = g->groups_next)
        if (strcmp(g->filename, n) == 0)
            return;

    avahi_log_info("Loading service file %s", n);
    
    g = static_service_group_new(n);
    if (static_service_group_load(g) < 0) {
        avahi_log_error("Failed to load service group file %s, ignoring.", g->filename);
        static_service_group_free(g);
    }
}

void static_service_load(void) {
    StaticServiceGroup *g, *n;
    glob_t globbuf;
    gchar **p;

    for (g = groups; g; g = n) {
        struct stat st;

        n = g->groups_next;

        if (stat(g->filename, &st) < 0) {

            if (errno == ENOENT)
                avahi_log_info("Service group file %s vanished, removing services.", g->filename);
            else
                avahi_log_warn("Failed to stat() file %s, ignoring: %s", g->filename, strerror(errno));
            
            static_service_group_free(g);
        } else if (st.st_mtime != g->mtime) {
            avahi_log_info("Service group file %s changed, reloading.", g->filename);
            
            if (static_service_group_load(g) < 0) {
                avahi_log_warn("Failed to load service group file %s, removing service.", g->filename);
                static_service_group_free(g);
            }
        }
    }

    memset(&globbuf, 0, sizeof(globbuf));
    if (glob(AVAHI_SERVICE_DIRECTORY "/*.service", GLOB_ERR, NULL, &globbuf) != 0)
        avahi_log_error("glob() failed.\n");
    else {
        for (p = globbuf.gl_pathv; *p; p++)
            load_file(*p);
        
        globfree(&globbuf);
    }
}

void static_service_free_all(void) {
    
    while (groups)
        static_service_group_free(groups);
}

void static_service_add_to_server(void) {
    StaticServiceGroup *g;

    for (g = groups; g; g = g->groups_next)
        add_static_service_group_to_server(g);
}

void static_service_remove_from_server(void) {
    StaticServiceGroup *g;

    for (g = groups; g; g = g->groups_next)
        remove_static_service_group_from_server(g);
}

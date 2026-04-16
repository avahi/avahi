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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <glob.h>
#include <unistd.h>
#include <sys/stat.h>

#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-core/core.h>
#include <avahi-core/log.h>
#include <avahi-core/util.h>

#include "ini-file-parser.h"

/** Append an item to the linked list
 *
 *  This is analogous (a counter-part) to AVAHI_LLIST_PREPEND in
 *  'avahi-common/llist.h', but added here intentionally to not expose more
 *  surface in the public header.
 */
#define AVAHI_LLIST_APPEND(t,name,head,item) do { \
                                        t **_head = &(head), *_item = (item); \
                                        t **iter; \
                                        assert(_item); \
                                        _item->name##_next = NULL; \
                                        if (!head) { *_head = item; item->name##_prev = NULL; break; } \
                                        for (iter = &(head); *iter && (*iter)->name##_next; iter = &((*iter)->name##_next)) { ; } \
                                        (*iter)->name##_next = _item; \
                                        _item->name##_prev = *iter; \
                                        } while (0)

char **avahi_ini_list_confd_files_sorted(const char *confd_path, int *confd_file_count) {
    DIR *dir = NULL;
    char **filelist = NULL;
    int filelist_count = 0;
    size_t glob_match_count = 0;
    glob_t glob_buf;
    int glob_ret = 0;
    char glob_pattern[PATH_MAX*2];

    /* initialize for early returns before the count gets set */
    *confd_file_count = 0;

    dir = opendir(confd_path);
    if (!dir) {
        if (errno == ENOENT) {
            /* not having conf.d in a system is fine, and probably the norm */
            avahi_log_debug("Could not find avahi-daemon.conf.d directory '%s'", confd_path);
        } else
            avahi_log_warn("Could not open '%s': %s", confd_path, strerror(errno));

        return NULL;
    }
    closedir(dir);

    snprintf(glob_pattern, sizeof(glob_pattern), "%s/*.conf", confd_path);
    glob_ret = glob(glob_pattern, 0, NULL, &glob_buf);
    if (glob_ret != 0) {
        if (glob_ret == GLOB_NOMATCH)
            avahi_log_debug("No files found in avahi-daemon.conf.d directory '%s'", confd_path);
        else
            avahi_log_error("Error during glob operation reading '%s', insufficient memory or reading error", glob_pattern);

        globfree(&glob_buf);
        return NULL;
    }

    /* allocate (elem+1) with zeros, for null pointers if any element is
     * discarded and a final sentinel */
    glob_match_count = glob_buf.gl_pathc;
    avahi_log_debug("Found %zu files matching '%s'\n", glob_match_count, glob_pattern);
    filelist = avahi_malloc0((glob_match_count + 1) * sizeof(char *));

    for (size_t i = 0; i < glob_match_count; i++) {
        struct stat stat_buf;
        int stat_ret = stat(glob_buf.gl_pathv[i], &stat_buf);
        if (stat_ret != 0) {
            avahi_log_error("Error in stat() for file in conf.d '%s': %s", glob_buf.gl_pathv[i], strerror(errno));
            continue;
        }

        if (stat_buf.st_size == 0) {
            avahi_log_debug("Discarding file in conf.d '%s' (empty)", glob_buf.gl_pathv[i]);
            continue;
        }

        switch (stat_buf.st_mode & S_IFMT) {
           case S_IFLNK:
           case S_IFREG:
               avahi_log_debug("Considering file in conf.d '%s'", glob_buf.gl_pathv[i]);
               filelist[filelist_count] = avahi_strdup(glob_buf.gl_pathv[i]);
               filelist_count++;
               break;
           case S_IFDIR:
               avahi_log_debug("Discarding file in conf.d '%s' (it is a directory)", glob_buf.gl_pathv[i]);
               continue; /* 'break' not necessary */
           case S_IFBLK:
           case S_IFCHR:
           case S_IFIFO:
           case S_IFSOCK:
           default:
               avahi_log_debug("Discarding file in conf.d '%s' (block/char device, socket or others)", glob_buf.gl_pathv[i]);
               continue; /* 'break' not necessary */
        }
    }

    *confd_file_count = filelist_count;
    globfree(&glob_buf);

    if (filelist_count == 0) {
        avahi_log_debug("No valid or matchig files found in avahi-daemon.conf.d directory '%s'", confd_path);
        for (size_t i = 0; i < glob_match_count; i++) {
            avahi_free(filelist[i]);
            filelist[i] = NULL;
        }
        avahi_free(filelist);
        return NULL;
    }

    avahi_log_debug("Sorted list of conf.d files:");
    for (int i = 0; i < filelist_count; i++)
        avahi_log_debug("- %s", filelist[i]);

    return filelist;
}

AvahiIniFile* avahi_ini_file_load(const char *fname) {
    AvahiIniFile *f;
    FILE *fo;
    AvahiIniFileGroup *group = NULL;
    unsigned line;

    assert(fname);

    if (!(fo = fopen(fname, "r"))) {
        avahi_log_error("Failed to open file '%s': %s", fname, strerror(errno));
        return NULL;
    }

    f = avahi_new(AvahiIniFile, 1);
    AVAHI_LLIST_HEAD_INIT(AvahiIniFileGroup, f->groups);
    f->n_groups = 0;

    line = 0;
    while (!feof(fo)) {
        char ln[1024], *s, *e;
        AvahiIniFilePair *pair;

        if (!(fgets(ln, sizeof(ln), fo)))
            break;

        line++;

        s = ln + strspn(ln, " \t");
        s[strcspn(s, "\r\n")] = 0;

        /* Skip comments and empty lines */
        if (*s == '#' || *s == '%' || *s == 0)
            continue;

        if (*s == '[') {
            /* new group */

            if (!(e = strchr(s, ']'))) {
                avahi_log_error("Unclosed group header in %s:%u: <%s>", fname, line, s);
                goto fail;
            }

            *e = 0;

            group = avahi_new(AvahiIniFileGroup, 1);
            group->name = avahi_strdup(s+1);
            group->n_pairs = 0;
            AVAHI_LLIST_HEAD_INIT(AvahiIniFilePair, group->pairs);

            AVAHI_LLIST_APPEND(AvahiIniFileGroup, groups, f->groups, group);
            f->n_groups++;
        } else {

            /* Normal assignment */
            if (!(e = strchr(s, '='))) {
                avahi_log_error("Missing assignment in %s:%u: <%s>", fname, line, s);
                goto fail;
            }

            if (!group) {
                avahi_log_error("Assignment outside group in %s:%u <%s>", fname, line, s);
                goto fail;
            }

            /* Split the key and the value */
            *(e++) = 0;

            pair = avahi_new(AvahiIniFilePair, 1);
            pair->key = avahi_strdup(s);
            pair->value = avahi_strdup(e);

            AVAHI_LLIST_APPEND(AvahiIniFilePair, pairs, group->pairs, pair);
            group->n_pairs++;
        }
    }

    fclose(fo);

    return f;

fail:

    if (fo)
        fclose(fo);

    if (f)
        avahi_ini_file_free(f);

    return NULL;
}

void avahi_ini_file_free(AvahiIniFile *f) {
    AvahiIniFileGroup *g;
    assert(f);

    while ((g = f->groups)) {
        AvahiIniFilePair *p;

        while ((p = g->pairs)) {
            avahi_free(p->key);
            avahi_free(p->value);

            AVAHI_LLIST_REMOVE(AvahiIniFilePair, pairs, g->pairs, p);
            avahi_free(p);
        }

        avahi_free(g->name);

        AVAHI_LLIST_REMOVE(AvahiIniFileGroup, groups, f->groups, g);
        avahi_free(g);
    }

    avahi_free(f);
}

char** avahi_split_csv(const char *t) {
    unsigned n_comma = 0;
    const char *p;
    char **r, **i;

    for (p = t; *p; p++)
        if (*p == ',')
            n_comma++;

    i = r = avahi_new(char*, n_comma+2);

    for (;;) {
        size_t n, l = strcspn(t, ",");
        const char *c;

        /* Ignore leading blanks */
        for (c = t, n = l; isblank(*c); c++, n--);

        /* Ignore trailing blanks */
        for (; n > 0 && isblank(c[n-1]); n--);

        *(i++) = avahi_strndup(c, n);

        t += l;

        if (*t == 0)
            break;

        assert(*t == ',');
        t++;
    }

    *i = NULL;

    return r;
}

void avahi_strfreev(char **p) {
    char **i;

    if (!p)
        return;

    for (i = p; *i; i++)
        avahi_free(*i);

    avahi_free(p);
}

static char *get_machine_id(void) {
    int fd;
    const int machine_id_length = 32;
    char buf[machine_id_length + 1];

    fd = open("/etc/machine-id", O_RDONLY|O_CLOEXEC|O_NOCTTY);
    if (fd == -1 && errno == ENOENT)
        fd = open("/var/lib/dbus/machine-id", O_RDONLY|O_CLOEXEC|O_NOCTTY);
    if (fd == -1)
        return NULL;

    /* File is on a filesystem so we never get EINTR or partial reads */
    if (read(fd, buf, machine_id_length) != machine_id_length) {
        close(fd);
        return NULL;
    }
    close(fd);

    /* end with NULL for future string use */
    buf[32] = '\0';

    /* Contents can be lower, upper and even mixed case so normalize */
    avahi_strdown(buf);

    return avahi_strndup(buf, sizeof buf);
}

static int is_yes(const char *s) {
    assert(s);

    return *s == 'y' || *s == 'Y' || *s == '1' || *s == 't' || *s == 'T';
}

static int parse_unsigned(const char *s, unsigned *u) {
    char *e = NULL;
    unsigned long ul;
    unsigned k;

    errno = 0;
    ul = strtoul(s, &e, 0);

    if (!e || *e || errno != 0)
        return -1;

    k = (unsigned) ul;

    if ((unsigned long) k != ul)
        return -1;

    *u = k;
    return 0;
}

static int parse_usec(const char *s, AvahiUsec *u) {
    char *e = NULL;
    unsigned long long ull;
    AvahiUsec k;

    errno = 0;
    ull = strtoull(s, &e, 0);

    if (!e || *e || errno != 0)
        return -1;

    k = (AvahiUsec) ull;

    if ((unsigned long long) k != ull)
        return -1;

    *u = k;
    return 0;
}

AvahiStringList *avahi_ini_filter_duplicate_domains(AvahiStringList *l) {
    AvahiStringList *e, *n, *p;

    if (!l)
        return l;

    for (p = l, e = l->next; e; e = n) {
        n = e->next;

        if (avahi_domain_equal((char*) e->text, (char*) l->text)) {
            p->next = e->next;
            avahi_free(e);
        } else
            p = e;
    }

    l->next = avahi_ini_filter_duplicate_domains(l->next);
    return l;
}

int avahi_ini_file_parse(DaemonConfig *c, const char *config_file) {
    int r = -1;
    AvahiIniFile *f;
    AvahiIniFileGroup *g;

    assert(c);
    assert(config_file);

    if (!(f = avahi_ini_file_load(config_file))) {
        avahi_log_error("Could not load config file: '%s'", config_file);
        goto finish;
    }

    for (g = f->groups; g; g = g->groups_next) {

        if (strcasecmp(g->name, "server") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {

                if (strcasecmp(p->key, "host-name") == 0) {
                    /* save given host-name, c->host_name will be set at the end */
                    if (c->host_name_given)
                        avahi_free(c->host_name_given);
                    c->host_name_given = avahi_strdup(p->value);
                    avahi_log_debug("host-name: given '%s'", c->host_name_given);
                } else if (strcasecmp(p->key, "host-name-from-machine-id") == 0) {
                    if (is_yes(p->value)) {
                        avahi_log_debug("host-name-from-machine-id: set to 'TRUE'");
                        c->host_name_from_machine_id = 1;
                    } else {
                        avahi_log_debug("host-name-from-machine-id: set to 'FALSE'");
                        c->host_name_from_machine_id = 0;
                    }
                } else if (strcasecmp(p->key, "domain-name") == 0) {
                    avahi_free(c->server_config.domain_name);
                    c->server_config.domain_name = avahi_strdup(p->value);
                } else if (strcasecmp(p->key, "browse-domains") == 0) {
                    char **e, **t;
                    char *line;

                    avahi_log_debug("browse-domains: processing line: '%s'", p->value);
                    e = avahi_split_csv(p->value);
                    if (!e || (e[0] && strcmp(e[0], "") == 0)) {
                        /* reset if the string is null or empty */
                        avahi_log_debug("browse-domains reset");

                        avahi_string_list_free(c->server_config.browse_domains);
                        c->server_config.browse_domains = NULL;
                    } else {
                        for (t = e; *t; t++) {
                            char cleaned[AVAHI_DOMAIN_NAME_MAX];

                            if (!avahi_normalize_name(*t, cleaned, sizeof(cleaned))) {
                                avahi_log_error("Invalid domain name \"%s\" for key \"%s\" in group \"%s\"\n", *t, p->key, g->name);
                                avahi_strfreev(e);
                                goto finish;
                            }

                            avahi_log_debug("browse-domains: adding '%s'", cleaned);
                            c->server_config.browse_domains = avahi_string_list_add(c->server_config.browse_domains, cleaned);
                        }
                    }
                    avahi_strfreev(e);

                    c->server_config.browse_domains = avahi_ini_filter_duplicate_domains(c->server_config.browse_domains);

                    line = avahi_string_list_to_string(c->server_config.browse_domains);
                    avahi_log_debug("browse-domains: after processing complete line: '%s'", line);
                    avahi_free(line);

                } else if (strcasecmp(p->key, "use-ipv4") == 0)
                    c->server_config.use_ipv4 = is_yes(p->value);
                else if (strcasecmp(p->key, "use-ipv6") == 0)
                    c->server_config.use_ipv6 = is_yes(p->value);
                else if (strcasecmp(p->key, "check-response-ttl") == 0)
                    c->server_config.check_response_ttl = is_yes(p->value);
                else if (strcasecmp(p->key, "allow-point-to-point") == 0)
                    c->server_config.allow_point_to_point = is_yes(p->value);
                else if (strcasecmp(p->key, "use-iff-running") == 0)
                    c->server_config.use_iff_running = is_yes(p->value);
                else if (strcasecmp(p->key, "disallow-other-stacks") == 0)
                    c->server_config.disallow_other_stacks = is_yes(p->value);
#ifdef HAVE_DBUS
                else if (strcasecmp(p->key, "enable-dbus") == 0) {

                    if (*(p->value) == 'w' || *(p->value) == 'W') {
                        c->fail_on_missing_dbus = 0;
                        c->enable_dbus = 1;
                    } else if (*(p->value) == 'y' || *(p->value) == 'Y') {
                        c->fail_on_missing_dbus = 1;
                        c->enable_dbus = 1;
                    } else {
                        c->enable_dbus = 0;
                    }
                }
#endif
                else if (strcasecmp(p->key, "allow-interfaces") == 0) {
                    char **e, **t;
                    char *line;

                    avahi_log_debug("allow-interfaces: processing line: '%s'", p->value);
                    e = avahi_split_csv(p->value);
                    if (!e || (e[0] && strcmp(e[0], "") == 0)) {
                        /* reset if the string is null or empty */
                        line = avahi_string_list_to_string(c->server_config.allow_interfaces);
                        avahi_log_debug("allow-interfaces reset, content was: '%s'", line);
                        avahi_free(line);

                        avahi_string_list_free(c->server_config.allow_interfaces);
                        c->server_config.allow_interfaces = NULL;
                    } else {
                        for (t = e; *t; t++) {
                            if (t[0] && strcmp(t[0], "") != 0) {
                                avahi_log_debug("allow-interfaces: adding '%s'", t[0]);
                                c->server_config.allow_interfaces = avahi_string_list_add(c->server_config.allow_interfaces, *t);
                            } else
                                avahi_log_debug("allow-interfaces: skipping empty value '%s'", t[0]);
                        }
                    }
                    avahi_strfreev(e);

                    line = avahi_string_list_to_string(c->server_config.allow_interfaces);
                    avahi_log_debug("allow-interfaces: after processing complete line: '%s'", line);
                    avahi_free(line);

                } else if (strcasecmp(p->key, "deny-interfaces") == 0) {
                    char **e, **t;
                    char *line;

                    avahi_log_debug("deny-interfaces: processing line: '%s'", p->value);
                    e = avahi_split_csv(p->value);
                    if (!e || (e[0] && strcmp(e[0], "") == 0)) {
                        /* reset if the string is null or empty */
                        line = avahi_string_list_to_string(c->server_config.deny_interfaces);
                        avahi_log_debug("deny-interfaces reset, content was: '%s'", line);
                        avahi_free(line);

                        avahi_string_list_free(c->server_config.deny_interfaces);
                        c->server_config.deny_interfaces = NULL;
                    } else {
                        for (t = e; *t; t++)
                            if (t[0] && strcmp(t[0], "") != 0) {
                                avahi_log_debug("deny-interfaces: adding '%s'", t[0]);
                                c->server_config.deny_interfaces = avahi_string_list_add(c->server_config.deny_interfaces, *t);
                            } else
                                avahi_log_debug("deny-interfaces: skipping empty value '%s'", t[0]);
                    }
                    avahi_strfreev(e);

                    line = avahi_string_list_to_string(c->server_config.deny_interfaces);
                    avahi_log_debug("deny-interfaces: after processing complete line: '%s'", line);
                    avahi_free(line);

                } else if (strcasecmp(p->key, "ratelimit-interval-usec") == 0) {
                    AvahiUsec k;

                    if (parse_usec(p->value, &k) < 0) {
                        avahi_log_error("Invalid ratelimit-interval-usec setting %s", p->value);
                        goto finish;
                    }

                    c->server_config.ratelimit_interval = k;

                } else if (strcasecmp(p->key, "ratelimit-burst") == 0) {
                    unsigned k;

                    if (parse_unsigned(p->value, &k) < 0) {
                        avahi_log_error("Invalid ratelimit-burst setting %s", p->value);
                        goto finish;
                    }

                    c->server_config.ratelimit_burst = k;

                } else if (strcasecmp(p->key, "cache-entries-max") == 0) {
                    unsigned k;

                    if (parse_unsigned(p->value, &k) < 0) {
                        avahi_log_error("Invalid cache-entries-max setting %s", p->value);
                        goto finish;
                    }

                    c->server_config.n_cache_entries_max = k;
#ifdef HAVE_DBUS
                } else if (strcasecmp(p->key, "clients-max") == 0) {
                    unsigned k;

                    if (parse_unsigned(p->value, &k) < 0) {
                        avahi_log_error("Invalid clients-max setting %s", p->value);
                        goto finish;
                    }

                    c->n_clients_max = k;
                } else if (strcasecmp(p->key, "objects-per-client-max") == 0) {
                    unsigned k;

                    if (parse_unsigned(p->value, &k) < 0) {
                        avahi_log_error("Invalid objects-per-client-max setting %s", p->value);
                        goto finish;
                    }

                    c->n_objects_per_client_max = k;
                } else if (strcasecmp(p->key, "entries-per-entry-group-max") == 0) {
                    unsigned k;

                    if (parse_unsigned(p->value, &k) < 0) {
                        avahi_log_error("Invalid entries-per-entry-group-max setting %s", p->value);
                        goto finish;
                    }

                    c->n_entries_per_entry_group_max = k;
#endif
                } else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }

        } else if (strcasecmp(g->name, "publish") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {

                if (strcasecmp(p->key, "publish-addresses") == 0)
                    c->server_config.publish_addresses = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-hinfo") == 0)
                    c->server_config.publish_hinfo = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-workstation") == 0)
                    c->server_config.publish_workstation = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-domain") == 0)
                    c->server_config.publish_domain = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-resolv-conf-dns-servers") == 0)
                    c->publish_resolv_conf = is_yes(p->value);
                else if (strcasecmp(p->key, "disable-publishing") == 0)
                    c->server_config.disable_publishing = is_yes(p->value);
                else if (strcasecmp(p->key, "disable-user-service-publishing") == 0)
                    c->disable_user_service_publishing = is_yes(p->value);
                else if (strcasecmp(p->key, "add-service-cookie") == 0)
                    c->server_config.add_service_cookie = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-dns-servers") == 0) {
                    char **e;

                    avahi_log_debug("publish-dns-servers: processing line: '%s'", p->value);
                    e = avahi_split_csv(p->value);
                    if (!e || (e[0] && strcmp(e[0], "") == 0)) {
                        /* reset if the string is null or empty */
                        avahi_log_debug("publish-dns-servers reset");

                        avahi_strfreev(c->publish_dns_servers);
                        c->publish_dns_servers = NULL;
                        avahi_strfreev(e);
                    } else {
                        char **t;

                        avahi_strfreev(c->publish_dns_servers);
                        c->publish_dns_servers = e;
                        for (t = e; *t; t++) {
                            if (t[0] && strcmp(t[0], "") != 0)
                                avahi_log_debug("publish_dns_servers: adding '%s'", t[0]);
                            else
                                avahi_log_debug("publish_dns_servers: skipping empty value '%s'", t[0]);
                        }
                    }
                } else if (strcasecmp(p->key, "publish-a-on-ipv6") == 0)
                    c->server_config.publish_a_on_ipv6 = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-aaaa-on-ipv4") == 0)
                    c->server_config.publish_aaaa_on_ipv4 = is_yes(p->value);
                else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }

        } else if (strcasecmp(g->name, "wide-area") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {

                if (strcasecmp(p->key, "enable-wide-area") == 0)
                    c->server_config.enable_wide_area = is_yes(p->value);
                else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }

        } else if (strcasecmp(g->name, "reflector") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {

                if (strcasecmp(p->key, "enable-reflector") == 0)
                    c->server_config.enable_reflector = is_yes(p->value);
                else if (strcasecmp(p->key, "reflect-ipv") == 0)
                    c->server_config.reflect_ipv = is_yes(p->value);
                else if (strcasecmp(p->key, "reflect-filters") == 0) {
                    char **e, **t;
                    char *line;

                    avahi_log_debug("reflect-filters: processing line: '%s'", p->value);
                    e = avahi_split_csv(p->value);
                    if (!e || (e[0] && strcmp(e[0], "") == 0)) {
                        /* reset if the string is null or empty */
                        avahi_log_debug("reflect-filters reset");

                        avahi_string_list_free(c->server_config.reflect_filters);
                        c->server_config.reflect_filters = NULL;
                    } else {
                        for (t = e; *t; t++) {
                            if (t[0] && strcmp(t[0], "") != 0) {
                                avahi_log_debug("reflect-filters: adding '%s'", t[0]);
                                c->server_config.reflect_filters = avahi_string_list_add(c->server_config.reflect_filters, *t);
                            } else
                                avahi_log_debug("reflect-filters: skipping empty value '%s'", t[0]);
                        }
                    }
                    avahi_strfreev(e);

                    line = avahi_string_list_to_string(c->server_config.reflect_filters);
                    avahi_log_debug("reflect-filters: after processing complete line: '%s'", line);
                    avahi_free(line);
                }
                else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }

        } else if (strcasecmp(g->name, "rlimits") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {

                if (strcasecmp(p->key, "rlimit-as") == 0) {
                    c->rlimit_as_set = 1;
                    c->rlimit_as = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-core") == 0) {
                    c->rlimit_core_set = 1;
                    c->rlimit_core = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-data") == 0) {
                    c->rlimit_data_set = 1;
                    c->rlimit_data = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-fsize") == 0) {
                    c->rlimit_fsize_set = 1;
                    c->rlimit_fsize = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-nofile") == 0) {
                    c->rlimit_nofile_set = 1;
                    c->rlimit_nofile = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-stack") == 0) {
                    c->rlimit_stack_set = 1;
                    c->rlimit_stack = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-nproc") == 0) {
#ifdef RLIMIT_NPROC
                    c->rlimit_nproc_set = 1;
                    c->rlimit_nproc = atoi(p->value);
#else
                    avahi_log_error("Ignoring configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
#endif
                } else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }

            }

        } else {
            avahi_log_error("Invalid configuration file group \"%s\".\n", g->name);
            goto finish;
        }
    }

    r = 0;

    /* set host-name based on config */
    if (c->host_name_from_machine_id == 1) {
        char *machine_id = NULL;

        if (c->host_name_given) {
            avahi_log_debug("host-name: host_name_from_machine_id=TRUE, ignoring config value host-name='%s'",
                            c->host_name_given);
        }

        avahi_free(c->server_config.host_name);
        machine_id = get_machine_id();
        if (machine_id) {
            c->server_config.host_name = machine_id;
            avahi_log_debug("host-name-from-machine-id: TRUE, host-name='%s'", c->server_config.host_name);
        } else {
            avahi_log_error("host-name-from-machine-id: cannot get machine_id");
            goto finish;
        }
    } else {
        if (c->host_name_given) {
            avahi_log_debug("host-name: setting given '%s'", c->host_name_given);
            avahi_free(c->server_config.host_name);
            c->server_config.host_name = avahi_strdup(c->host_name_given);
        } else
            avahi_log_debug("host-name: no host-name-from-machine-id and no explicit host-name set");
    }

finish:

    if (f)
        avahi_ini_file_free(f);

    return r;
}

int avahi_ini_load_all_config(DaemonConfig *config, const char *main_config_file) {
    char confd_path[PATH_MAX];
    char **confd_files = NULL;
    int confd_files_count = 0;
    long unsigned snprintf_count = 0;
    int r = -1;

    avahi_log_debug("Loading main conf: '%s'", main_config_file);
    r = avahi_ini_file_parse(config, main_config_file);
    if (r < 0) {
        avahi_log_error("Could not parse main file: '%s'", main_config_file);
        return r;
    }

    snprintf_count = snprintf(confd_path, sizeof(confd_path), "%s.d", main_config_file);
    if (snprintf_count >= sizeof(confd_path)) {
        avahi_log_error("File path of confd_path too long, truncated: '%s'", confd_path);
        r = -1;
        goto finish;
    }

    confd_files = avahi_ini_list_confd_files_sorted(confd_path, &confd_files_count);
    if (confd_files && confd_files_count > 0) {
        avahi_log_debug("Loading conf.d files in: '%s'", confd_path);
        for (int i = 0; i < confd_files_count; i++) {
            const char *confd_file = confd_files[i];
            avahi_log_debug("- %s", confd_file);
            if (avahi_ini_file_parse(config, confd_file) < 0) {
                avahi_log_error("Could not load conf.d file: '%s'", confd_file);
                r = -1;
                goto finish;
            }
        }
    } else
        avahi_log_debug("No valid conf.d files in '%s' found", confd_path);

    r = 0;

finish:
    if (confd_files) {
        for (int i = 0; i < confd_files_count; i++) {
            avahi_free(confd_files[i]);
            confd_files[i] = NULL;
        }
        avahi_free(confd_files);
    }

    return r;
}

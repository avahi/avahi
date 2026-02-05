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

#include <avahi-common/malloc.h>
#include <avahi-core/log.h>

#include "ini-file-parser.h"

#define AVAHI_INI_CONFD_MAX_FILES 1024

static int avahi_ini_filename_compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

char** avahi_ini_list_confd_files_sorted(const char* confd_path, int* confd_file_count) {
    int filename_len = 0;
    int suffix_len = 0;
    const char *suffix = ".conf";

    DIR *dir = NULL;
    struct dirent *dentry = NULL;
    int error_reading_dir = 0;
    char** filelist = NULL;
    int filelist_count = 0;

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

    filelist = avahi_malloc0(AVAHI_INI_CONFD_MAX_FILES * sizeof(char *));

    suffix_len = strlen(suffix);
    while ((dentry = readdir(dir)) != NULL) {
        filename_len = strlen(dentry->d_name);
        if ((dentry->d_name[0] != '.') &&
            (filename_len > suffix_len) &&
            (strcmp(dentry->d_name + (filename_len-suffix_len), suffix) == 0)) {
            if (filelist_count >= AVAHI_INI_CONFD_MAX_FILES) {
                avahi_log_error("Max number of files %d reached when processing: %s",
                                AVAHI_INI_CONFD_MAX_FILES, confd_path);
                error_reading_dir = 1;
                break;
            }

            filelist[filelist_count] = strdup(dentry->d_name);
            if (!filelist[filelist_count]) {
                avahi_log_error("strdup() error: %s", strerror(errno));
                error_reading_dir = 1;
                break;
            }

            avahi_log_debug("Considering file in conf.d '%s'", filelist[filelist_count]);

            filelist_count++;
        } else
            avahi_log_debug("Discarding file in conf.d '%s'", dentry->d_name);
    }
    closedir(dir);

    if (error_reading_dir) {
        avahi_strfreev(filelist);
        *confd_file_count = 0;
        return NULL;
    }

    qsort(filelist, filelist_count, sizeof(char *), avahi_ini_filename_compare);
    avahi_log_debug("Sorted list of conf.d files:");
    for (int i = 0; i < filelist_count; i++)
        avahi_log_debug("- %s", filelist[i]);

    *confd_file_count = filelist_count;

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

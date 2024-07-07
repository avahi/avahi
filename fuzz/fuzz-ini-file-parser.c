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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "avahi-common/malloc.h"
#include "avahi-core/log.h"
#include "avahi-daemon/ini-file-parser.h"

void log_function(AvahiLogLevel level, const char *txt) {}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char name[] = "/tmp/fuzz-ini-file-parser.XXXXXX";
    int fd = -1;
    ssize_t n;
    AvahiIniFile *f;
    AvahiIniFileGroup *g;

    fd = mkstemp(name);
    assert(fd >= 0);

    n = write(fd, data, size);
    assert(n == (ssize_t) size);

    close(fd);

    avahi_set_log_function(log_function);

    if (!(f = avahi_ini_file_load(name)))
        goto cleanup;

    avahi_log_info("%u groups\n", f->n_groups);

    for (g = f->groups; g; g = g->groups_next) {
        AvahiIniFilePair *p;
        avahi_log_info("<%s> (%u pairs)\n", g->name, g->n_pairs);

        for (p = g->pairs; p; p = p->pairs_next) {
            char **split, **i;

            avahi_log_info("\t<%s> = ", p->key);
            split = avahi_split_csv(p->value);

            for (i = split; *i; i++)
                avahi_log_info("<%s> ", *i);

            avahi_strfreev(split);

            avahi_log_info("\n");
        }
    }

cleanup:
    if (f)
        avahi_ini_file_free(f);
    unlink(name);

    return 0;
}

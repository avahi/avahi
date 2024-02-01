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
#include <stdint.h>
#include <stddef.h>

#include "avahi-common/malloc.h"
#include "avahi-common/strlst.h"
#include "avahi-common/utf8.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    AvahiStringList *a = NULL, *b = NULL;
    uint8_t *rdata = NULL;
    char *t = NULL;
    size_t s, n;
    int ret;

    if (avahi_string_list_parse(data, size, &a) < 0)
        goto finish;

    if ((t = avahi_string_list_to_string(a)))
        assert(avahi_utf8_valid(t));

    avahi_free(avahi_string_list_to_string(a));
    avahi_string_list_get_service_cookie(a);

    n = avahi_string_list_serialize(a, NULL, 0);
    assert(n > 0);
    if (!(rdata = avahi_malloc(n)))
        goto finish;

    s = avahi_string_list_serialize(a, rdata, n);
    assert(s == n);

    if (avahi_string_list_parse(rdata, n, &b) < 0)
        goto finish;

    ret = avahi_string_list_equal(a, b);
    assert(ret);

    avahi_string_list_free(b);
    if (!(b = avahi_string_list_copy(a)))
        goto finish;

    ret = avahi_string_list_equal(a, b);
    assert(ret);

finish:
    avahi_free(t);
    avahi_free(rdata);
    if (b)
        avahi_string_list_free(b);
    if (a)
        avahi_string_list_free(a);

    return 0;
}

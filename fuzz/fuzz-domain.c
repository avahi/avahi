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
#include <string.h>

#include "avahi-common/malloc.h"
#include "avahi-common/domain.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *s = NULL, *t = NULL;

    if(!(s = avahi_malloc(size+1)))
        return 0;

    memcpy(s, data, size);
    s[size] = '\0';

    if ((t = avahi_normalize_name_strdup(s)))
        assert(avahi_domain_equal(s, t));

    avahi_is_valid_service_type_generic(s);
    avahi_is_valid_service_type_strict(s);
    avahi_is_valid_service_subtype(s);
    avahi_is_valid_domain_name(s);
    avahi_is_valid_service_name(s);
    avahi_is_valid_host_name(s);
    avahi_is_valid_fqdn(s);

    avahi_free(t);
    avahi_free(s);

    return 0;
}

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

#include <assert.h>

#include <avahi-common/gccmacro.h>

#include "utf8.h"

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {

    assert(avahi_utf8_valid("hallo"));
    assert(avahi_utf8_valid("1234567890."));
    /* same word in iso-8859-1 as utf-8 below. */
    assert(!avahi_utf8_valid("\xfcxkn\xfcrz"));
    assert(avahi_utf8_valid("üxknürz"));
    assert(avahi_utf8_valid("žluťoučký kůň pěl ďábelské ódy"));
    /* few examples from https://www.iana.org/domains/reserved */
    assert(avahi_utf8_valid("испытание"));
    assert(avahi_utf8_valid("δοκιμή"));
    assert(avahi_utf8_valid("テスト"));

    return 0;
}

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

    /* Functional tests */
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
    /* Valid 4-byte UTF-8 characters */
    assert(avahi_utf8_valid("😀"));                /* Grinning Face emoji U+1F600 */
    assert(avahi_utf8_valid("\xF0\x9F\x92\xA9"));  /* PILE OF POO emoji */

    /* Negative functional tests for line coverage */
    /* Overlong encodings (invalid) */
    assert(!avahi_utf8_valid("\xC0\xAF"));         /* '/' overlong */
    assert(!avahi_utf8_valid("\xE0\x80\x80"));     /* NUL overlong */
    assert(!avahi_utf8_valid("\xF0\x80\x80\x80")); /* NUL overlong (4-byte) */
    /* Bad continuation bytes */
    assert(!avahi_utf8_valid("\xC2\x20"));         /* continuation must be 10xxxxxx */
    assert(!avahi_utf8_valid("\xE2\x28\xA1"));     /* second byte invalid */
    assert(!avahi_utf8_valid("\xF0\x28\x8C\xBC"));
    /* Illegal leading bytes */
    assert(!avahi_utf8_valid("\x80"));             /* continuation as leader */
    assert(!avahi_utf8_valid("\xFF"));             /* invalid UTF-8 byte */
    assert(!avahi_utf8_valid("\xFE"));
    /* UTF-16 surrogate halves (U+D800–U+DFFF) are invalid codepoints in UTF-8 */
    assert(!avahi_utf8_valid("\xED\xA0\x80"));     /* U+D800 */
    assert(!avahi_utf8_valid("\xED\xBF\xBF"));     /* U+DFFF */
    /* Unicode noncharacters (codepoints never assigned to characters) */
    assert(!avahi_utf8_valid("\xEF\xBF\xBE"));     /* U+FFFE */
    assert(!avahi_utf8_valid("\xEF\xBF\xBF"));     /* U+FFFF */
    /* Above Unicode max */
    assert(!avahi_utf8_valid("\xF4\x90\x80\x80")); /* U+110000 */
    /* Truncated UTF-8 sequences */
    assert(!avahi_utf8_valid("\xE2\x82"));         /* missing 3rd byte */
    assert(!avahi_utf8_valid("\xF0\x9F\x92"));     /* missing 4th byte */

    /* Directed tests to ensure branch coverage */
    /* Boundary around UTF-16 surrogate block */
    assert(avahi_utf8_valid("\xED\x9F\xBF"));      /* U+D7FF (valid Hangul Jamo Extended-B) */
    assert(avahi_utf8_valid("\xEE\x80\x80"));      /* U+E000 (valid Private Use Area)  */
    /* Boundary around noncharacter range */
    assert(avahi_utf8_valid("\xEF\xB7\x8F"));      /* U+FDCF (valid Arabic) */
    assert(!avahi_utf8_valid("\xEF\xB7\x90"));     /* U+FDD0 (invalid noncharacter) */
    assert(!avahi_utf8_valid("\xEF\xB7\xAF"));     /* U+FDEF (invalid noncharacter) */
    assert(avahi_utf8_valid("\xEF\xB7\xB0"));      /* U+FDF0 (valid Arabic) */

    return 0;
}

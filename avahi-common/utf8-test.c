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

#include <avahi-common/test-util.h>

#include <avahi-common/gccmacro.h>

#include "utf8.h"

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {

    /* Functional tests */
    must(avahi_utf8_valid("hallo"));
    must(avahi_utf8_valid("1234567890."));
    /* same word in iso-8859-1 as utf-8 below. */
    must(!avahi_utf8_valid("\xfcxkn\xfcrz"));
    must(avahi_utf8_valid("üxknürz"));
    must(avahi_utf8_valid("žluťoučký kůň pěl ďábelské ódy"));
    /* few examples from https://www.iana.org/domains/reserved */
    must(avahi_utf8_valid("испытание"));
    must(avahi_utf8_valid("δοκιμή"));
    must(avahi_utf8_valid("テスト"));
    /* Valid 4-byte UTF-8 characters */
    must(avahi_utf8_valid("😀"));                /* Grinning Face emoji U+1F600 */
    must(avahi_utf8_valid("\xF0\x9F\x92\xA9"));  /* PILE OF POO emoji */

    /* Negative functional tests for line coverage */
    /* Overlong encodings (invalid) */
    must(!avahi_utf8_valid("\xC0\xAF"));         /* '/' overlong */
    must(!avahi_utf8_valid("\xE0\x80\x80"));     /* NUL overlong */
    must(!avahi_utf8_valid("\xF0\x80\x80\x80")); /* NUL overlong (4-byte) */
    /* Bad continuation bytes */
    must(!avahi_utf8_valid("\xC2\x20"));         /* continuation must be 10xxxxxx */
    must(!avahi_utf8_valid("\xE2\x28\xA1"));     /* second byte invalid */
    must(!avahi_utf8_valid("\xF0\x28\x8C\xBC"));
    /* Illegal leading bytes */
    must(!avahi_utf8_valid("\x80"));             /* continuation as leader */
    must(!avahi_utf8_valid("\xFF"));             /* invalid UTF-8 byte */
    must(!avahi_utf8_valid("\xFE"));
    /* UTF-16 surrogate halves (U+D800–U+DFFF) are invalid codepoints in UTF-8 */
    must(!avahi_utf8_valid("\xED\xA0\x80"));     /* U+D800 */
    must(!avahi_utf8_valid("\xED\xBF\xBF"));     /* U+DFFF */
    /* Unicode noncharacters (codepoints never assigned to characters) */
    must(!avahi_utf8_valid("\xEF\xBF\xBE"));     /* U+FFFE */
    must(!avahi_utf8_valid("\xEF\xBF\xBF"));     /* U+FFFF */
    /* Above Unicode max */
    must(!avahi_utf8_valid("\xF4\x90\x80\x80")); /* U+110000 */
    /* Truncated UTF-8 sequences */
    must(!avahi_utf8_valid("\xE2\x82"));         /* missing 3rd byte */
    must(!avahi_utf8_valid("\xF0\x9F\x92"));     /* missing 4th byte */

    /* Directed tests to ensure branch coverage */
    /* Boundary around UTF-16 surrogate block */
    must(avahi_utf8_valid("\xED\x9F\xBF"));      /* U+D7FF (valid Hangul Jamo Extended-B) */
    must(avahi_utf8_valid("\xEE\x80\x80"));      /* U+E000 (valid Private Use Area)  */
    /* Boundary around noncharacter range */
    must(avahi_utf8_valid("\xEF\xB7\x8F"));      /* U+FDCF (valid Arabic) */
    must(!avahi_utf8_valid("\xEF\xB7\x90"));     /* U+FDD0 (invalid noncharacter) */
    must(!avahi_utf8_valid("\xEF\xB7\xAF"));     /* U+FDEF (invalid noncharacter) */
    must(avahi_utf8_valid("\xEF\xB7\xB0"));      /* U+FDF0 (valid Arabic) */

    return 0;
}

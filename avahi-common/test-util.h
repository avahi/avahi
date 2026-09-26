#ifndef footestutilhfoo
#define footestutilhfoo

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

/** \file test-util.h Checks for the test programs */

#include <stdio.h>
#include <stdlib.h>

/* Test programs check with must(), not assert(). NDEBUG removes
 * assert(), and a test that checks nothing passes. must() evaluates
 * its argument in every build, so it may also wrap a call the test
 * depends on. It flushes stdout before it aborts so that the test's
 * earlier output reaches the log, which buffers stdout fully. */
#define must(expr)                                                      \
    do {                                                                \
        if (!(expr)) {                                                  \
            fflush(stdout);                                             \
            fprintf(stderr, "%s:%d: %s: must(%s) failed\n",             \
                    __FILE__, __LINE__, __func__, #expr);               \
            abort();                                                    \
        }                                                               \
    } while (0)

#endif

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

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "timeval.h"

#if defined(HAVE_CLOCK_GETTIME) && defined(_POSIX_MONOTONIC_CLOCK)
#define USE_MONOTONIC 1
#else
#define USE_MONOTONIC 0
#endif

int avahi_timeval_compare(const struct AvahiTimeVal *a, const struct AvahiTimeVal *b) {
    assert(a);
    assert(b);

    if (a->tv_sec < b->tv_sec)
        return -1;

    if (a->tv_sec > b->tv_sec)
        return 1;

    if (a->tv_usec < b->tv_usec)
        return -1;

    if (a->tv_usec > b->tv_usec)
        return 1;

    return 0;
}

AvahiUsec avahi_timeval_diff(const struct AvahiTimeVal *a, const struct AvahiTimeVal *b) {
    assert(a);
    assert(b);

    if (avahi_timeval_compare(a, b) < 0)
        return - avahi_timeval_diff(b, a);

    return ((AvahiUsec) a->tv_sec - b->tv_sec)*1000000 + a->tv_usec - b->tv_usec;
}

struct AvahiTimeVal* avahi_timeval_add(struct AvahiTimeVal *a, AvahiUsec usec) {
    AvahiUsec u;
    assert(a);

    u = usec + a->tv_usec;

    if (u < 0) {
        a->tv_usec = (long) (1000000 + (u % 1000000));
        a->tv_sec += (long) (-1 + (u / 1000000));
    } else {
        a->tv_usec = (long) (u % 1000000);
        a->tv_sec += (long) (u / 1000000);
    }

    return a;
}

struct AvahiTimeVal* avahi_now(struct AvahiTimeVal *now) {
    struct timeval tv;
#if USE_MONOTONIC
    struct timespec ts;
#endif

    assert(now);

#if USE_MONOTONIC
    /* Use a monotonic clock if possible. This prevents jumps in the system
     * clock from messing with timers (especially jumps backward) */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        now->tv_sec = ts.tv_sec;
        now->tv_usec = ts.tv_nsec / 1000;
    } else {
#endif
        gettimeofday(&tv, NULL);
        now->tv_sec = tv.tv_sec;
        now->tv_usec = tv.tv_usec;
#if USE_MONOTONIC
    }
#endif

    return now;
}

AvahiUsec avahi_age(const struct AvahiTimeVal *a) {
    struct AvahiTimeVal now;

    assert(a);

    return avahi_timeval_diff(avahi_now(&now), a);
}

struct AvahiTimeVal *avahi_elapse_time(struct AvahiTimeVal *tv, unsigned msec, unsigned jitter) {
    assert(tv);

    avahi_now(tv);

    if (msec)
        avahi_timeval_add(tv, (AvahiUsec) msec*1000);

    if (jitter) {
        static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        static int last_rand;
        static time_t timestamp = 0;

        time_t now;
        int r;

        now = time(NULL);

        pthread_mutex_lock(&mutex);
        if (now >= timestamp + 10) {
            timestamp = now;
            last_rand = rand();
        }

        r = last_rand;

        pthread_mutex_unlock(&mutex);

        /* We use the same jitter for 10 seconds. That way our
         * time events elapse in bursts which has the advantage that
         * packet data can be aggregated better */

        avahi_timeval_add(tv, (AvahiUsec) (jitter*1000.0*r/(RAND_MAX+1.0)));
    }

    return tv;
}


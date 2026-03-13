#ifndef QAVAHI_H
#define QAVAHI_H

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
  License along with avahi; if not, see <https://www.gnu.org/licenses/>.
***/

/** \file qt-watch.h Qt main loop adapter */

#include <avahi-common/watch.h>

AVAHI_C_DECL_BEGIN

/** Setup abstract poll structure for integration with Qt main loop  */
const AvahiPoll* avahi_qt_poll_get(void)
#ifdef HAVE_VISIBILITY_HIDDEN
__attribute__ ((visibility("default")))
#endif
;

AVAHI_C_DECL_END

#endif

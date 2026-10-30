#ifndef foochroothelperhfoo
#define foochroothelperhfoo

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

#include <stdio.h>

int avahi_chroot_helper_start(const char *argv0);
void avahi_chroot_helper_shutdown(void);
int avahi_chroot_helper_get(const char *fname);

int avahi_chroot_helper_get_fd(const char *fname);
FILE *avahi_chroot_helper_get_file(const char *fname);

int avahi_chroot_helper_unlink(const char *fname);

#endif

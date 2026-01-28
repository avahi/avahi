#ifndef fooinifileparserhfoo
#define fooinifileparserhfoo

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

#include <avahi-common/llist.h>

typedef struct AvahiIniFile AvahiIniFile;
typedef struct AvahiIniFilePair AvahiIniFilePair;
typedef struct AvahiIniFileGroup AvahiIniFileGroup;

struct AvahiIniFilePair {
    AVAHI_LLIST_FIELDS(AvahiIniFilePair, pairs);
    char *key, *value;
};

struct AvahiIniFileGroup {
    AVAHI_LLIST_FIELDS(AvahiIniFileGroup, groups);
    char *name;

    AVAHI_LLIST_HEAD(AvahiIniFilePair, pairs);
    unsigned n_pairs;
};

struct AvahiIniFile {
    AVAHI_LLIST_HEAD(AvahiIniFileGroup, groups);
    unsigned n_groups;
};


AvahiIniFile* avahi_ini_file_load(const char *fname);
void avahi_ini_file_free(AvahiIniFile *f);

/**
 * Get a list of files from conf.d-style dir (config fragments), sorted, to be
 * loaded over the base configuration file.
 *
 * @confd_path: directory to load files from.  It should be typically named
 * after the config file used as base, for example if the main config file is in
 * '/etc/avahi/avahi-daemon.conf' this variale should be
 * '/etc/avahi/avahi-daemon.conf.d'.
 *
 * @confd_file_count: variable that will be set to the number of valid files
 * being read from that directory, the same as the elements of the array
 * returned
 *
 * @return array of filenames within the given @confd_path
 */
char** avahi_ini_list_confd_files_sorted(const char* confd_path, int* confd_file_count);

char** avahi_split_csv(const char *t);

void avahi_strfreev(char **);

#endif

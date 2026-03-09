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
#include <avahi-common/strlst.h>

#include <avahi-core/core.h>

#include <sys/resource.h>

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

typedef enum {
    DAEMON_RUN,
    DAEMON_KILL,
    DAEMON_VERSION,
    DAEMON_HELP,
    DAEMON_RELOAD,
    DAEMON_CHECK
} DaemonCommand;

typedef struct {
    AvahiServerConfig server_config;
    DaemonCommand command;
    int daemonize;
    int use_syslog;
    char *config_file;
#ifdef HAVE_DBUS
    int enable_dbus;
    int fail_on_missing_dbus;
    unsigned n_clients_max;
    unsigned n_objects_per_client_max;
    unsigned n_entries_per_entry_group_max;
#endif
    int drop_root;
    int set_rlimits;
#ifdef ENABLE_CHROOT
    int use_chroot;
#endif
    int modify_proc_title;

    int disable_user_service_publishing;
    int publish_resolv_conf;
    char ** publish_dns_servers;
    int debug;

    int rlimit_as_set, rlimit_core_set, rlimit_data_set, rlimit_fsize_set, rlimit_nofile_set, rlimit_stack_set;
    rlim_t rlimit_as, rlimit_core, rlimit_data, rlimit_fsize, rlimit_nofile, rlimit_stack;

#ifdef RLIMIT_NPROC
    int rlimit_nproc_set;
    rlim_t rlimit_nproc;
#endif
} DaemonConfig;

/** Remove duplicate domains from the given list */
AvahiStringList *avahi_ini_filter_duplicate_domains(AvahiStringList *l);

/**
 * Parse a config file and load daemon configuration.
 *
 * @c: store the config from the parsed config file.
 *
 * @config_file: load this config file.
 *
 * @return error if < 0
 */
int avahi_ini_file_parse(DaemonConfig *c, const char* config_file);

/**
 * Load all config files (main config plus associated conf.d) and into daemon configuration.
 *
 * @c: store the config from the parsed config file.
 *
 * @main_config_file: main config file, conf.d will be derived from it appending
 * ".d" to the same path.
 *
 * @return error if < 0
 */
int avahi_ini_load_all_config(DaemonConfig *config, const char* main_config_file);

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

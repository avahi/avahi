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

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <avahi-common/malloc.h>
#include <avahi-core/log.h>

#include "ini-file-parser.h"

unsigned int MAX_LOG_LEVEL = AVAHI_LOG_DEBUG;

static char test_confd_temp_dir[PATH_MAX];

/******************************************************************************/
/* Helpers                                                                    */
/******************************************************************************/
static void print_test_name(const char *name) {
    avahi_log_info("\n--------------------------------------------------------------------------------\n"
                   "Test: <%s>", name);
}

static void print_ini_file(AvahiIniFile *f) {
    AvahiIniFileGroup *g;

    printf("%u groups\n", f->n_groups);

    for (g = f->groups; g; g = g->groups_next) {
        AvahiIniFilePair *p;
        printf("<%s> (%u pairs)\n", g->name, g->n_pairs);

        for (p = g->pairs; p; p = p->pairs_next) {
            char **split, **i;

            printf("\t<%s> = ", p->key);
            split = avahi_split_csv(p->value);

            for (i = split; *i; i++)
                printf("<%s> ", *i);

            avahi_strfreev(split);

            printf("\n");
        }
    }
}

static int write_file(const char *filename, const char *content) {
    FILE *file = NULL;
    size_t written = 0;

    file = fopen(filename, "w");
    if (file == NULL) {
        avahi_log_error("error: opening file '%s' to write: %s", filename, strerror(errno));
        return -1;
    }

    written = fwrite(content, sizeof(char), strlen(content), file);
    if (written < strlen(content)) {
        avahi_log_error("error: writing to file '%s': %s", filename, strerror(errno));
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int write_confd_file(const char *filename, const char *content) {
    char dest_file[PATH_MAX*2];

    snprintf(dest_file, sizeof(dest_file), "%s/avahi-daemon.conf.d/%s", test_confd_temp_dir, filename);

    return write_file(dest_file, content);
}

static int remove_verbose(const char *path) {
    avahi_log_debug("debug: remove_verbose(%s)", path);
    return remove(path);
}

static int rmdir_force_recursive(const char *dirname) {
    const struct dirent *entry;
#if !defined(__linux__)
    struct stat statbuf;
#endif
    DIR *dp = NULL;

    avahi_log_debug("debug: rmdir_force_recursive(%s)", dirname);

    dp = opendir(dirname);
    if (dp == NULL) {
        avahi_log_error("error: opening dir '%s': %s", dirname, strerror(errno));
        return -1;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

#if defined(__linux__)
            if (entry->d_type == DT_DIR) {
#else
            if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
#endif
                int ret = 0;
                ret = rmdir_force_recursive(path);
                if (ret < 0) {
                    avahi_log_error("error: removing recursively directory '%s': %s", path, strerror(errno));
                    closedir(dp);
                    return -1;
                }
            } else {
                if (remove_verbose(path) < 0) {
                    avahi_log_error("error: removing file '%s': %s", path, strerror(errno));
                    closedir(dp);
                    return -1;
                }
            }
        }
    }

    closedir(dp);

    if (rmdir(dirname) != 0) {
        avahi_log_error("error: removing dir '%s': %s", dirname, strerror(errno));
        return -1;
    }

    return 0;
}

static int test_confd_setup_create_temp_dir(void) {
    char test_confd_temp_dir_templ[] = "/tmp/avahi-daemon-test.XXXXXX";

    if (mkdtemp(test_confd_temp_dir_templ) == NULL) {
        if (errno == EEXIST)
            avahi_log_error("error: dir '%s' already exists", test_confd_temp_dir);
        else
            avahi_log_error("error: creating dir '%s': %s", test_confd_temp_dir, strerror(errno));

        return -1;
    }

    snprintf(test_confd_temp_dir, sizeof(test_confd_temp_dir), "%s", test_confd_temp_dir_templ);
    avahi_log_info("info: temporary dir created: '%s'", test_confd_temp_dir);

    return 0;
}

static int test_confd_setup_write_main_conf_file(void) {
    char dest_file[PATH_MAX*2];
    const char *content =
        "# This file is part of avahi.\n"
        "\n"
        "[server]\n"
        "#host-name=foo\n"
        "#host-name-from-machine-id=no\n"
        "#domain-name=local\n"
        "#browse-domains=0pointer.de, zeroconf.org\n"
        "use-ipv4=yes\n"
        "use-ipv6=yes\n"
        "#allow-interfaces=eth0\n"
        "#deny-interfaces=eth1\n"
        "#check-response-ttl=no\n"
        "#use-iff-running=no\n"
        "#enable-dbus=yes\n"
        "#disallow-other-stacks=no\n"
        "#allow-point-to-point=no\n"
        "#cache-entries-max=4096\n"
        "#clients-max=4096\n"
        "#objects-per-client-max=1024\n"
        "#entries-per-entry-group-max=32\n"
        "ratelimit-interval-usec=1000000\n"
        "ratelimit-burst=1000\n"
        "\n"
        "[wide-area]\n"
        "#enable-wide-area=no\n"
        "\n"
        "[publish]\n"
        "#disable-publishing=no\n"
        "#disable-user-service-publishing=no\n"
        "#add-service-cookie=no\n"
        "#publish-addresses=yes\n"
        "publish-hinfo=no\n"
        "publish-workstation=no\n"
        "#publish-domain=yes\n"
        "#publish-dns-servers=192.168.50.1, 192.168.50.2\n"
        "#publish-resolv-conf-dns-servers=yes\n"
        "#publish-aaaa-on-ipv4=yes\n"
        "#publish-a-on-ipv6=no\n"
        "\n"
        "[reflector]\n"
        "#enable-reflector=no\n"
        "#reflect-ipv=no\n"
        "#reflect-filters=_airplay._tcp.local,_raop._tcp.local\n"
        "\n"
        "[rlimits]\n"
        "#rlimit-as=\n"
        "#rlimit-core=0\n"
        "#rlimit-data=8388608\n"
        "#rlimit-fsize=0\n"
        "#rlimit-nofile=768\n"
        "#rlimit-stack=8388608\n"
        "#rlimit-nproc=3\n";

    snprintf(dest_file, sizeof(dest_file), "%s/avahi-daemon.conf", test_confd_temp_dir);

    return write_file(dest_file, content);
}

static int test_confd_setup_create_confd(void) {
    char confd_dir[PATH_MAX*2];

    /* create conf.d */
    snprintf(confd_dir, sizeof(confd_dir), "%s/avahi-daemon.conf.d", test_confd_temp_dir);
    if (mkdir(confd_dir, 0755) != 0) {
        if (errno == EEXIST)
            avahi_log_error("error: dir '%s' already exists", confd_dir);
        else
            avahi_log_error("error: creating dir '%s': %s", confd_dir, strerror(errno));

        return -1;
    }

    return 0;
}

static int test_confd_setup(void) {

    if (test_confd_setup_create_temp_dir() < 0) {
        avahi_log_error("error: problem creating temporary directory '%s'", test_confd_temp_dir);
        return -1;
    }

    if (test_confd_setup_write_main_conf_file() < 0) {
        avahi_log_error("error: problem writing main conf file to temporary directory '%s'", test_confd_temp_dir);
        return -1;
    }

    if (test_confd_setup_create_confd() < 0) {
        avahi_log_error("error: problem creating conf.d in '%s'", test_confd_temp_dir);
        return -1;
    }

    return 0;
}

static int test_confd_teardown(void) {
    avahi_log_info("info: cleaning up temp directory recursively: '%s'", test_confd_temp_dir);
    return rmdir_force_recursive(test_confd_temp_dir);
}

static int test_confd_helper_load_all_config(DaemonConfig *config) {
    char main_config_file[PATH_MAX*2];

    snprintf(main_config_file, sizeof(main_config_file), "%s/avahi-daemon.conf", test_confd_temp_dir);

    if (avahi_ini_load_all_config(config, main_config_file) < 0) {
        avahi_log_error("error: could not load config based on main file '%s'", main_config_file);
        return -1;
    }

    return 0;
}

/******************************************************************************/
/* Tests                                                                      */
/******************************************************************************/
static int test_print_config(void) {
    int r = -1;
    char dest_file[PATH_MAX*2];
    AvahiIniFile *f;

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    snprintf(dest_file, sizeof(dest_file), "%s/avahi-daemon.conf", test_confd_temp_dir);

    if (!(f = avahi_ini_file_load(dest_file)))
        goto finish;

    print_ini_file(f);
    avahi_ini_file_free(f);

    r = 0;

finish:
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_avahi_ini_file_load_non_existing(void) {
    int r = -1;
    char dest_file[PATH_MAX*2];
    AvahiIniFile *f = NULL;

    print_test_name(__func__);

    if (test_confd_setup_create_temp_dir() < 0) {
        avahi_log_error("error: problem creating temporary directory '%s'", test_confd_temp_dir);
        goto finish;
    }

    snprintf(dest_file, sizeof(dest_file), "%s/DOES-NOT-EXIST", test_confd_temp_dir);

    f = avahi_ini_file_load(dest_file);
    if (f != NULL) {
        avahi_log_error("error: trying to load non-existing file did not return error");
        avahi_ini_file_free(f);
        goto finish;
    }

    r = 0;

finish:
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_avahi_ini_file_parse_malformed(void) {
    int r = -1;
    DaemonConfig config = {0};
    char dest_file[PATH_MAX*2];

    print_test_name(__func__);

    if (test_confd_setup_create_temp_dir() < 0) {
        avahi_log_error("error: problem creating temporary directory '%s'", test_confd_temp_dir);
        goto finish;
    }

    if (test_confd_setup_create_confd() < 0) {
        avahi_log_error("error: problem creating conf.d in '%s'", test_confd_temp_dir);
        goto finish;
    }

    if (write_confd_file("../malformed.conf",
                         "Avahi Daemon Test\n") < 0)
        goto finish;

    snprintf(dest_file, sizeof(dest_file), "%s/malformed.conf", test_confd_temp_dir);
    if (avahi_ini_file_parse(&config, dest_file) >= 0) {
        avahi_log_error("error: trying to parse malformed file did not return error");
        goto finish;
    }

    r = 0;

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_no_confd(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup_create_temp_dir() < 0 || test_confd_setup_write_main_conf_file() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests without conf.d");
        goto finish;
    }

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: problem loading config");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.browse_domains == NULL) {
        r = 0;
        avahi_log_info("info: empty browse_domains list does match the expectations");
    } else
        avahi_log_error("error: non-empty browse_domains list does not match the expectations");

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_empty(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: problem loading config");
        goto finish;
    }

    /* check the expected values */
    if ((config.server_config.use_ipv4 == 1)
        && (config.server_config.use_ipv6 == 1)
        && (config.server_config.ratelimit_interval == 1000000)
        && (config.server_config.ratelimit_burst == 1000)) {
        r = 0;
        avahi_log_info("info: config values match the expectations");
    } else
        avahi_log_error("error: some config values do not match the expectations");

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_expect_files(void) {
    int r = -1;
    char dest_file[PATH_MAX/4];
    char confd_path[PATH_MAX*2];
    const int confd_fragments = 13;
    char **confd_files_sorted;
    int confd_file_count;
    time_t current_time;

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    time(&current_time);
    for (int i = 1; i <= confd_fragments; i++) {
        /* create the files somewhat shuffled, not in ascending order based on i */
        snprintf(dest_file, sizeof(dest_file), "test-%02li.conf", ((i + current_time) % confd_fragments) + 1);

        if (write_confd_file(dest_file, "# test file") < 0)
            return -1;
    }

    confd_file_count = 0;
    snprintf(confd_path, sizeof(confd_path), "%s/avahi-daemon.conf.d", test_confd_temp_dir);
    confd_files_sorted = avahi_ini_list_confd_files_sorted(confd_path, &confd_file_count);

    if (confd_file_count == 13 &&
        strcmp(basename(confd_files_sorted[ 0]), "test-01.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 1]), "test-02.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 2]), "test-03.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 3]), "test-04.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 4]), "test-05.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 5]), "test-06.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 6]), "test-07.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 7]), "test-08.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 8]), "test-09.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 9]), "test-10.conf") == 0 &&
        strcmp(basename(confd_files_sorted[10]), "test-11.conf") == 0 &&
        strcmp(basename(confd_files_sorted[11]), "test-12.conf") == 0 &&
        strcmp(basename(confd_files_sorted[12]), "test-13.conf") == 0) {
        avahi_log_info("info: conf.d files present and sorted as expected");
        r = 0;
    } else
        avahi_log_error("error: error getting conf.d number (13 expected, got %i) or sorting", confd_file_count);

    for (int i = 0; i < confd_file_count; i++) {
        avahi_free(confd_files_sorted[i]);
        confd_files_sorted[i] = NULL;
    }
    avahi_free(confd_files_sorted);

finish:
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_invalid_conf_filenames(void) {
    int r = -1;
    char confd_path[PATH_MAX*2];
    char **confd_files_sorted;
    int confd_file_count;
    char conf_file_as_dir[PATH_MAX*2];
    char conf_file_as_symlink[PATH_MAX*2];

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    if (write_confd_file("test-22-invald.conf~", "# test file") < 0)
        return -1;
    if (write_confd_file("test-55.conf", "# test file") < 0)
        return -1;
    if (write_confd_file("test-11.conf", "# test file") < 0)
        return -1;
    if (write_confd_file(".conf", "# test file") < 0)
        return -1;
    if (write_confd_file("test-44-invald.conf.bak", "# test file") < 0)
        return -1;
    if (write_confd_file("test-33.conf", "# test file") < 0)
        return -1;
    if (write_confd_file("test-66-invald.foo", "# test file") < 0)
        return -1;

    /* creating .conf as directory, should be ignored */
    snprintf(conf_file_as_dir, sizeof(conf_file_as_dir), "%s/avahi-daemon.conf.d/test-99-is-dir.conf", test_confd_temp_dir);
    if (mkdir(conf_file_as_dir, 0755) != 0) {
        avahi_log_error("error: creating dir '%s': %s", conf_file_as_dir, strerror(errno));
        return -1;
    }

    /* creating .conf as directory, should be ignored */
    snprintf(conf_file_as_symlink, sizeof(conf_file_as_symlink), "%s/avahi-daemon.conf.d/test-zz-is-symlink-to-11.conf", test_confd_temp_dir);
    if (symlink("test-11.conf", conf_file_as_symlink) != 0) {
        avahi_log_error("error: creating symlink '%s': %s", conf_file_as_symlink, strerror(errno));
        return -1;
    }

    confd_file_count = 0;
    snprintf(confd_path, sizeof(confd_path), "%s/avahi-daemon.conf.d", test_confd_temp_dir);
    confd_files_sorted = avahi_ini_list_confd_files_sorted(confd_path, &confd_file_count);

    if (confd_file_count == 4 &&
        strcmp(basename(confd_files_sorted[ 0]), "test-11.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 1]), "test-33.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 2]), "test-55.conf") == 0 &&
        strcmp(basename(confd_files_sorted[ 3]), "test-zz-is-symlink-to-11.conf") == 0) {
        avahi_log_info("info: conf.d files present and sorted as expected");
        r = 0;
    } else
        avahi_log_error("error: error getting conf.d number (3 expected, got %i) or sorting", confd_file_count);

    for (int i = 0; i < confd_file_count; i++) {
        avahi_free(confd_files_sorted[i]);
        confd_files_sorted[i] = NULL;
    }
    avahi_free(confd_files_sorted);

finish:
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_invalid_contents(void) {
    int r = 0;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        r = -1;
        goto finish;
    }

    /* for different tests, expected that loading malformed config fails */

    if (write_confd_file("test.conf",
                         "[GarbageGroup]\n"
                         "GarbageKey=GarbageValue\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (garbage group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server]\n"
                         "ServerGarbageKey=GarbageValue\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (server group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[wide-area]\n"
                         "WideAreaGarbageKey=GarbageValue\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (wide-area group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[publish]\n"
                         "PublishGarbageKey=GarbageValue\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (publish group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[reflector]\n"
                         "ReflectorGarbageKey=GarbageValue\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (reflector group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[rlimits]\n"
                         "RlimitsGarbageKey=GarbageValue\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (rlimit group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server\n") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (group not closed properly) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server]\n"
                         "use_ipv4") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (key not assigned properly) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server]\n"
                         "use_ipv4:yes") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (key not assigned properly) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "use_ipv4=yes") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (assignement outside group) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server]\n"
                         "ratelimit-interval-usec=1M") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (parsing usec) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server]\n"
                         "ratelimit-burst=1thousand") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (parsing unsigned) but config loaded OK, unexpectedly");
        r = -1;
    }

    if (write_confd_file("test.conf",
                         "[server]\n"
                         "cache-entries-max=a-lot") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (parsing unsigned) but config loaded OK, unexpectedly");
        r = -1;
    }

#ifdef HAVE_DBUS
    if (write_confd_file("test.conf",
                         "[server]\n"
                         "clients-max=a-lot") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (parsing unsigned) but config loaded OK, unexpectedly");
        r = -1;
    }
    if (write_confd_file("test.conf",
                         "[server]\n"
                         "objects-per-client-max=a-lot") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (parsing unsigned) but config loaded OK, unexpectedly");
        r = -1;
    }
    if (write_confd_file("test.conf",
                         "[server]\n"
                         "entries-per-entry-group-max=a-lot") < 0)
        r = -1;
    if (test_confd_helper_load_all_config(&config) >= 0) {
        avahi_log_error("error: malformed content (parsing unsigned) but config loaded OK, unexpectedly");
        r = -1;
    }
#endif

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_browse_domains(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-browse-domains.conf",
                         "[server]\n"
                         "browse-domains=ephemerous.example.com\n") < 0)
        goto finish;
    if (write_confd_file("20-browse-domains.conf",
                         "[server]\n"
                         "browse-domains=\n"
                         "browse-domains=subdom1.example.com,subdom2.example.com\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.browse_domains != NULL) {
        char *domain_list = avahi_string_list_to_string(config.server_config.browse_domains);
        if (strcmp("\"subdom1.example.com\" \"subdom2.example.com\"", domain_list) == 0) {
            r = 0;
            avahi_log_info("info: browse_domains list '%s' does match the expectations", domain_list);
        } else
            avahi_log_error("error: browse_domains list '%s' does not match the expectations", domain_list);

        free(domain_list);
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_browse_domains_duplicate(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-browse-domains.conf",
                         "[server]\n"
                         "browse-domains=ephemerous.example.com\n") < 0)
        goto finish;
    if (write_confd_file("20-browse-domains.conf",
                         "[server]\n"
                         "browse-domains=\n"
                         "browse-domains=subdom1.example.com,subdom2.example.com,subdom2.example.com\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.browse_domains != NULL) {
        char *domain_list = avahi_string_list_to_string(config.server_config.browse_domains);
        if (strcmp("\"subdom1.example.com\" \"subdom2.example.com\"", domain_list) == 0) {
            r = 0;
            avahi_log_info("info: browse_domains list '%s' does match the expectations (removed duplicates)", domain_list);
        } else
            avahi_log_error("error: browse_domains list '%s' does not match the expectations", domain_list);

        free(domain_list);
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_allow_interfaces(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-allow-interfaces.conf",
                         "[server]\n"
                         "allow-interfaces=iface999\n") < 0)
        goto finish;
    if (write_confd_file("20-allow-interfaces.conf",
                         "[server]\n"
                         "allow-interfaces=\n"
                         "allow-interfaces=iface1,iface2,iface3\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.allow_interfaces != NULL) {
        char *interfaces_list = avahi_string_list_to_string(config.server_config.allow_interfaces);
        if (strcmp("\"iface1\" \"iface2\" \"iface3\"", interfaces_list) == 0) {
            r = 0;
            avahi_log_info("info: allow_interfaces list '%s' does match the expectations", interfaces_list);
        } else
            avahi_log_error("error: allow_interfaces list '%s' does not match the expectations", interfaces_list);

        free(interfaces_list);
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_deny_interfaces(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-deny-interfaces.conf",
                         "[server]\n"
                         "deny-interfaces=iface999\n") < 0)
        goto finish;
    if (write_confd_file("20-deny-interfaces.conf",
                         "[server]\n"
                         "deny-interfaces=\n"
                         "deny-interfaces=iface1,iface2,iface3,iface4\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.deny_interfaces != NULL) {
        char *interfaces_list = avahi_string_list_to_string(config.server_config.deny_interfaces);
        if (strcmp("\"iface1\" \"iface2\" \"iface3\" \"iface4\"", interfaces_list) == 0) {
            r = 0;
            avahi_log_info("info: deny_interfaces list '%s' does match the expectations", interfaces_list);
        } else
            avahi_log_error("error: deny_interfaces list '%s' does not match the expectations", interfaces_list);

        free(interfaces_list);
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_publish_dns_servers(void) {
    int r = -1;
    DaemonConfig config = {0};
    const size_t actual_config_size = 1024;
    char actual_config[actual_config_size];
    size_t actual_config_len = 0;

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-publish-dns-servers.conf",
                         "[publish]\n"
                         "publish-dns-servers=0.0.0.0\n") < 0)
        goto finish;
    if (write_confd_file("20-publish-dns-servers.conf",
                         "[publish]\n"
                         "publish-dns-servers=\n"
                         "publish-dns-servers=1.1.1.1, 2606:4700:4700::1111\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.publish_dns_servers != NULL) {
        const char *expected = "1.1.1.1, 2606:4700:4700::1111";

        for (int i = 0; config.publish_dns_servers[i] != NULL; i++) {
            int len = 0;
            len = snprintf(actual_config+actual_config_len,
                           sizeof(actual_config) - actual_config_len, "%s%s",
                           (actual_config_len > 0) ? ", " : "",
                           config.publish_dns_servers[i]);
            actual_config_len += len;
            if (actual_config_len >= actual_config_size) {
                avahi_log_error("error: publish-dns-servers '%s' truncated, something unexpected in the tests", actual_config);
                goto finish;
            }
        }

        if (strcmp(expected, actual_config) == 0) {
            r = 0;
            avahi_log_info("info: publish-dns-servers '%s' does match the expectations", actual_config);
        } else
            avahi_log_error("error: publish-dns-servers '%s' does not match the expectations", actual_config);
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_reflect_filters(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-reflect_filters.conf",
                         "[reflector]\n"
                         "reflect-filters=_INVALID._tcp.local\n") < 0)
        goto finish;
    if (write_confd_file("20-reflect_filters.conf",
                         "[reflector]\n"
                         "reflect-filters=\n"
                         "reflect-filters=_imaginary1._tcp.local,_imaginary2._tcp.local\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.reflect_filters != NULL) {
        char *filter_list = avahi_string_list_to_string(config.server_config.reflect_filters);
        if (strcmp("\"_imaginary1._tcp.local\" \"_imaginary2._tcp.local\"", filter_list) == 0) {
            r = 0;
            avahi_log_info("info: reflect-filters list '%s' does match the expectations", filter_list);
        } else
            avahi_log_error("error: reflect-filters list '%s' does not match the expectations", filter_list);

        free(filter_list);
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_get_machine_id(void) {
    int r = -1;
    DaemonConfig config = {0};
    int found_non_hex = 0;

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-exercise-server.conf",
                         "[server]\n"
                         "host-name=avahi-daemon-test\n"
                         "host-name-from-machine-id=yes\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if ((config.server_config.host_name != NULL) && strlen(config.server_config.host_name) == 32) {
        for (int i = 0; i < 32; i++) {
            char c = config.server_config.host_name[i];
            if (! ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                found_non_hex++;
        }
    }

    if (found_non_hex == 0) {
        avahi_log_info("info: [server] host-name-from-machine-id=yes seems to work, hostname='%s' does match the expectations",
                       config.server_config.host_name);

        r = 0;
    } else
        avahi_log_error("error: [server] host-name-from-machine-id=yes does not seem to work, hostname='%s' does not match the expectations",
                        config.server_config.host_name);

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_host_name_from_machine_id_yes(void) {
    int r = -1;
    DaemonConfig config = {0};
    int found_non_hex = 0;

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-host-name-from-machine-id-yes.conf",
                         "[server]\n"
                         "host-name=avahi-daemon-test\n"
                         "host-name-from-machine-id=yes\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if ((config.server_config.host_name != NULL) && strlen(config.server_config.host_name) == 32) {
        for (int i = 0; i < 32; i++) {
            char c = config.server_config.host_name[i];
            if (! ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                found_non_hex++;
        }
    }

    if (found_non_hex == 0) {
        avahi_log_info("info: [server] host-name-from-machine-id=yes seems to work, hostname='%s' does match the expectations",
                       config.server_config.host_name);

        r = 0;
    } else
        avahi_log_error("error: [server] host-name-from-machine-id=yes does not seem to work, hostname='%s' does not match the expectations",
                        config.server_config.host_name);

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_host_name_from_machine_id_no(void) {
    int r = -1;
    DaemonConfig config = {0};

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-host-name-from-machine-id-yes.conf",
                         "[server]\n"
                         "host-name=avahi-daemon-test\n"
                         "host-name-from-machine-id=yes\n") < 0)
        goto finish;

    if (write_confd_file("20-host-name-from-machine-id-no.conf",
                         "[server]\n"
                         "host-name-from-machine-id=no\n") < 0)
        goto finish;

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values */
    if (config.server_config.host_name != NULL && strcmp(config.server_config.host_name, "avahi-daemon-test") == 0) {
        avahi_log_info("info: [server] host-name-from-machine-id=no seems to work, hostname='%s' does match the expectations",
                       config.server_config.host_name);

        r = 0;
    } else
        avahi_log_error("error: [server] host-name-from-machine-id=yes does not seem to work, hostname='%s' does not match the expectations",
                        config.server_config.host_name);

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}

static int test_confd_exercise_all_keys(void) {
    int r = -1;
    int failed_to_write_files = 0;
    DaemonConfig config = {0};
    int unexpected = 0;

    print_test_name(__func__);

    if (test_confd_setup() < 0) {
        avahi_log_error("error: cannot set-up conf.d tests");
        goto finish;
    }

    /* write config fragment files */
    if (write_confd_file("10-exercise-server.conf",
                         "[server]\n"
                         "host-name=avahi-daemon-test\n"
                         "host-name-from-machine-id=no\n"
                         "domain-name=local\n"
                         "browse-domains=subdom1.example.com, subdom2.example.com\n"
                         "use-ipv4=yes\n"
                         "use-ipv6=yes\n"
                         "allow-interfaces=iface1allow\n"
                         "deny-interfaces=iface0deny\n"
                         "check-response-ttl=no\n"
                         "use-iff-running=no\n"
                         "disallow-other-stacks=no\n"
                         "allow-point-to-point=no\n"
                         "cache-entries-max=4444\n"
#ifdef HAVE_DBUS
                         "enable-dbus=yes\n"
                         "clients-max=1111\n"
                         "objects-per-client-max=555\n"
                         "entries-per-entry-group-max=33\n"
#endif
                         "ratelimit-interval-usec=999999\n"
                         "ratelimit-burst=999\n") < 0)
        failed_to_write_files++;
    if (write_confd_file("20-exercise-reflector.conf",
                         "[wide-area]\n"
                         "enable-wide-area=yes\n") < 0)
        failed_to_write_files++;
    if (write_confd_file("30-exercise-publish.conf",
                         "[publish]\n"
                         "disable-publishing=yes\n"
                         "disable-user-service-publishing=yes\n"
                         "add-service-cookie=yes\n"
                         "publish-addresses=yes\n"
                         "publish-hinfo=yes\n"
                         "publish-workstation=yes\n"
                         "publish-domain=no\n"
                         "publish-dns-servers=10.9.8.1, 10.9.8.2\n"
                         "publish-resolv-conf-dns-servers=no\n"
                         "publish-aaaa-on-ipv4=no\n"
                         "publish-a-on-ipv6=yes\n") < 0)
        failed_to_write_files++;
    if (write_confd_file("40-exercise-reflector.conf",
                         "[reflector]\n"
                         "enable-reflector=yes\n"
                         "reflect-ipv=yes\n"
                         "reflect-filters=_imaginary1._tcp.local,_imaginary2._tcp.local\n") < 0)
        failed_to_write_files++;
    if (write_confd_file("50-exercise-rlimits.conf",
                         "[rlimits]\n"
                         "rlimit-as=999999999\n"
                         "rlimit-core=888888888\n"
                         "rlimit-data=7777777\n"
                         "rlimit-fsize=6666666\n"
                         "rlimit-nofile=555\n"
                         "rlimit-stack=4444444\n"
                         "rlimit-nproc=333\n") < 0)
        failed_to_write_files++;

    if (failed_to_write_files > 0) {
        avahi_log_error("error: failed to write conf.d fragment files: %i", failed_to_write_files);
        goto finish;
    }

    if (test_confd_helper_load_all_config(&config) < 0) {
        avahi_log_error("error: test_confd_load_all_config() failed");
        goto finish;
    }

    /* check the expected values -- [server] */
    if (strcmp(config.server_config.host_name, "avahi-daemon-test") != 0) {
        avahi_log_error("error: [server] host-name '%s' does not match the expectations", config.server_config.host_name);
        unexpected++;
    }
    /* "[server] host-name-from-machine-id" does not correspond to a
     * code-variable, the visible result of being enabled is that the host-name
     * would be set from get_machine_id()
     */
    if (strcmp(config.server_config.domain_name, "local") != 0) {
        avahi_log_error("error: [server] domain-name '%s' does not match the expectations", config.server_config.domain_name);
        unexpected++;
    }
    if (config.server_config.browse_domains != NULL) {
        char *domain_list = avahi_string_list_to_string(config.server_config.browse_domains);
        if (strcmp("\"subdom1.example.com\" \"subdom2.example.com\"", domain_list) != 0) {
            avahi_log_error("error: [server] browse-domains list '%s' does not match the expectations", domain_list);
            unexpected++;
        }
        free(domain_list);
    } else {
        avahi_log_error("error: [server] empty browse-domains list does not match the expectations");
        unexpected++;
    }
    if (config.server_config.use_ipv4 != 1) {
        avahi_log_error("error: [server] use-ipv4 '%i' does not match the expectations", config.server_config.use_ipv4);
        unexpected++;
    }
    if (config.server_config.use_ipv6 != 1) {
        avahi_log_error("error: [server] use-ipv6 '%i' does not match the expectations", config.server_config.use_ipv6);
        unexpected++;
    }
    if (config.server_config.allow_interfaces != NULL) {
        char *iface_list = avahi_string_list_to_string(config.server_config.allow_interfaces);
        if (strcmp("\"iface1allow\"", iface_list) != 0) {
            avahi_log_error("error: [server] allow-interfaces list '%s' does not match the expectations", iface_list);
            unexpected++;
        }
        free(iface_list);
    } else {
        avahi_log_error("error: [server] empty allow-interfaces list does not match the expectations");
        unexpected++;
    }
    if (config.server_config.deny_interfaces != NULL) {
        char *iface_list = avahi_string_list_to_string(config.server_config.deny_interfaces);
        if (strcmp("\"iface0deny\"", iface_list) != 0) {
            avahi_log_error("error: [server] deny-interfaces list '%s' does not match the expectations", iface_list);
            unexpected++;
        }
        free(iface_list);
    } else {
        avahi_log_error("error: [server] empty deny-interfaces list does not match the expectations");
        unexpected++;
    }
    if (config.server_config.check_response_ttl != 0) {
        avahi_log_error("error: [server] check-response-ttl '%i' does not match the expectations", config.server_config.check_response_ttl);
        unexpected++;
    }
    if (config.server_config.use_iff_running != 0) {
        avahi_log_error("error: [server] use-iff-running '%i' does not match the expectations", config.server_config.use_iff_running);
        unexpected++;
    }
    if (config.server_config.disallow_other_stacks != 0) {
        avahi_log_error("error: [server] disallow-other-stacks '%i' does not match the expectations", config.server_config.disallow_other_stacks);
        unexpected++;
    }
    if (config.server_config.allow_point_to_point != 0) {
        avahi_log_error("error: [server] allow-point-to-point '%i' does not match the expectations", config.server_config.allow_point_to_point);
        unexpected++;
    }
    if (config.server_config.n_cache_entries_max != 4444) {
        avahi_log_error("error: [server] cache-entries-max '%u' does not match the expectations", config.server_config.n_cache_entries_max);
        unexpected++;
    }
#ifdef HAVE_DBUS
    if (config.enable_dbus != 1) {
        avahi_log_error("error: [server] enable-dbus '%i' does not match the expectations", config.enable_dbus);
        unexpected++;
    }
    if (config.n_clients_max != 1111) {
        avahi_log_error("error: [server] clients-max '%u' does not match the expectations", config.n_clients_max);
        unexpected++;
    }
    if (config.n_objects_per_client_max != 555) {
        avahi_log_error("error: [server] objects-per-client-max '%u' does not match the expectations", config.n_objects_per_client_max);
        unexpected++;
    }
    if (config.n_entries_per_entry_group_max != 33) {
        avahi_log_error("error: [server] entries-per-entry-group-max '%u' does not match the expectations", config.n_entries_per_entry_group_max);
        unexpected++;
    }
#endif
    if (config.server_config.ratelimit_interval != 999999) {
        avahi_log_error("error: [server] ratelimit-interval-usec '%lli' does not match the expectations", (long long int)config.server_config.ratelimit_interval);
        unexpected++;
    }
    if (config.server_config.ratelimit_burst != 999) {
        avahi_log_error("error: [server] ratelimit-burst '%u' does not match the expectations", config.server_config.ratelimit_burst);
        unexpected++;
    }

    /* check the expected values -- [wide-area] */
    if (config.server_config.enable_wide_area != 1) {
        avahi_log_error("error: [wide-area] enable-wide-area '%i' does not match the expectations", config.server_config.enable_wide_area);
        unexpected++;
    }

    /* check the expected values -- [publish] */
    if (config.server_config.disable_publishing != 1) {
        avahi_log_error("error: [publish] disable-publishing '%i' does not match the expectations", config.server_config.disable_publishing);
        unexpected++;
    }
    if (config.disable_user_service_publishing != 1) {
        avahi_log_error("error: [publish] disable-user-service-publishing '%i' does not match the expectations", config.disable_user_service_publishing);
        unexpected++;
    }
    if (config.server_config.add_service_cookie != 1) {
        avahi_log_error("error: [publish] add-service-cookie '%i' does not match the expectations", config.server_config.add_service_cookie);
        unexpected++;
    }
    if (config.server_config.publish_addresses != 1) {
        avahi_log_error("error: [publish] publish-addresses '%i' does not match the expectations", config.server_config.publish_addresses);
        unexpected++;
    }
    if (config.server_config.publish_hinfo != 1) {
        avahi_log_error("error: [publish] publish-hinfo '%i' does not match the expectations", config.server_config.publish_hinfo);
        unexpected++;
    }
    if (config.server_config.publish_workstation != 1) {
        avahi_log_error("error: [publish] publish-workstation '%i' does not match the expectations", config.server_config.publish_workstation);
        unexpected++;
    }
    if (config.server_config.publish_domain != 0) {
        avahi_log_error("error: [publish] publish-domain '%i' does not match the expectations", config.server_config.publish_domain);
        unexpected++;
    }
    if (config.publish_dns_servers == NULL) {
        avahi_log_error("error: [publish] empty publish-dns-servers list does not match the expectations");
        unexpected++;
    } else if ((config.publish_dns_servers[0] == NULL) ||
               (config.publish_dns_servers[1] == NULL) ||
               (config.publish_dns_servers[2] != NULL)) {
        avahi_log_error("error: [publish] publish-dns-servers list does not match the expectations (expected 2 elements)");
        unexpected++;
    } else if ((strcmp("10.9.8.1", config.publish_dns_servers[0]) != 0 && strcmp("10.9.8.2", config.publish_dns_servers[1]) != 0)) {
        avahi_log_error("error: [publish] publish-dns-servers list does not match the expectations (10.9.8.1, 10.9.8.2), its contents are: '(%s, %s)'",
                config.publish_dns_servers[0], config.publish_dns_servers[1]);
        unexpected++;
    }
    if (config.publish_resolv_conf != 0) {
        avahi_log_error("error: [publish] publish-resolv-conf-dns-servers '%i' does not match the expectations", config.publish_resolv_conf);
        unexpected++;
    }
    if (config.server_config.publish_aaaa_on_ipv4 != 0) {
        avahi_log_error("error: [publish] publish-aaaa-on-ipv4 '%i' does not match the expectations", config.server_config.publish_aaaa_on_ipv4);
        unexpected++;
    }
    if (config.server_config.publish_a_on_ipv6 != 1) {
        avahi_log_error("error: [publish] publish-a-on-ipv6 '%i' does not match the expectations", config.server_config.publish_a_on_ipv6);
        unexpected++;
    }

    /* check the expected values -- [reflector] */
    if (config.server_config.enable_reflector != 1) {
        avahi_log_error("error: [reflector] enable-reflector '%i' does not match the expectations", config.server_config.enable_reflector);
        unexpected++;
    }
    if (config.server_config.reflect_ipv != 1) {
        avahi_log_error("error: [reflector] reflect-ipv '%i' does not match the expectations", config.server_config.reflect_ipv);
        unexpected++;
    }
    if (config.server_config.reflect_filters != NULL) {
        char *filter_list = avahi_string_list_to_string(config.server_config.reflect_filters);
        if (strcmp("\"_imaginary1._tcp.local\" \"_imaginary2._tcp.local\"", filter_list) != 0) {
            avahi_log_error("error: [reflector] reflect-filters list '%s' does not match the expectations", filter_list);
            unexpected++;
        }
        free(filter_list);
    } else {
        avahi_log_error("error: [reflector] empty reflect-filters list does not match the expectations");
        unexpected++;
    }

    /* check the expected values -- [rlimits] */
    if (config.rlimit_as != 999999999 ) {
        avahi_log_error("error: [rlimits] rlimit-as '%lu' does not match the expectations", config.rlimit_as);
        unexpected++;
    }
    if (config.rlimit_core != 888888888) {
        avahi_log_error("error: [rlimits] rlimit-core '%lu' does not match the expectations", config.rlimit_core);
        unexpected++;
    }
    if (config.rlimit_data != 7777777) {
        avahi_log_error("error: [rlimits] rlimit-data '%lu' does not match the expectations", config.rlimit_data);
        unexpected++;
    }
    if (config.rlimit_fsize != 6666666) {
        avahi_log_error("error: [rlimits] rlimit-fsize '%lu' does not match the expectations", config.rlimit_fsize);
        unexpected++;
    }
    if (config.rlimit_nofile != 555) {
        avahi_log_error("error: [rlimits] rlimit-nofile '%lu' does not match the expectations", config.rlimit_nofile);
        unexpected++;
    }
    if (config.rlimit_stack != 4444444 ) {
        avahi_log_error("error: [rlimits] rlimit-stack '%lu' does not match the expectations", config.rlimit_stack);
        unexpected++;
    }
#ifdef RLIMIT_NPROC
    if (config.rlimit_nproc != 333) {
        avahi_log_error("error: [rlimits] rlimit-nproc '%lu' does not match the expectations", config.rlimit_nproc);
        unexpected++;
    }
#endif

    /* final check */
    if (unexpected > 0)
        avahi_log_error("error: %i unexpected values found in parsed config", unexpected);
    else {
        avahi_log_info("info: %i unexpected values found in parsed config", unexpected);
        r = 0;
    }

finish:
    avahi_daemon_config_free(&config);
    test_confd_teardown();
    avahi_log_info("Test finished: %s", (r >= 0 ? "OK" : "FAIL"));

    return r;
}


/******************************************************************************/
/* main()                                                                     */
/******************************************************************************/
static void log_function(AvahiLogLevel level, const char *txt) {
    if (level <= MAX_LOG_LEVEL)
        fprintf(stderr, "%s\n", txt);
}


typedef int (*funcptr)(void);

typedef struct {
    funcptr *funcs;
} func_struct;

static funcptr func_array[] = {
    test_print_config,
    test_avahi_ini_file_load_non_existing,
    test_avahi_ini_file_parse_malformed,
    test_confd_no_confd,
    test_confd_empty,
    test_confd_expect_files,
    test_confd_invalid_conf_filenames,
    test_confd_invalid_contents,
    test_confd_browse_domains,
    test_confd_browse_domains_duplicate,
    test_confd_allow_interfaces,
    test_confd_deny_interfaces,
    test_confd_publish_dns_servers,
    test_confd_reflect_filters,
    test_confd_get_machine_id,
    test_confd_host_name_from_machine_id_yes,
    test_confd_host_name_from_machine_id_no,
    test_confd_exercise_all_keys,
    NULL
};

func_struct tests = {
    func_array
};

int main(AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[]) {
    int count = 0, failed = 0;

    const char *env_MAX_LOG_LEVEL = getenv("AVAHI_INI_FILE_PARSER_TEST_MAX_LOG_LEVEL");
    if (env_MAX_LOG_LEVEL) {
        if (strncmp(env_MAX_LOG_LEVEL, "INFO", strlen("INFO")) == 0)
            MAX_LOG_LEVEL = AVAHI_LOG_INFO;
        else if (strncmp(env_MAX_LOG_LEVEL, "NOTICE", strlen("NOTICE")) == 0)
            MAX_LOG_LEVEL = AVAHI_LOG_NOTICE;
        else if (strncmp(env_MAX_LOG_LEVEL, "WARN", strlen("WARN")) == 0)
            MAX_LOG_LEVEL = AVAHI_LOG_WARN;
        else if (strncmp(env_MAX_LOG_LEVEL, "ERROR", strlen("ERROR")) == 0)
            MAX_LOG_LEVEL = AVAHI_LOG_ERROR;
    }

    avahi_set_log_function(log_function);

    for (; tests.funcs[count] != NULL; count++) {
        int status = tests.funcs[count]();
        if (status < 0) {
            failed++;
        }
    }

    avahi_log_info("\n--------------------------------------------------------------------------------\n"
                   "Summary: %i total tests, %i FAILed and %i succeeded", count, failed, (count - failed));

    return (failed > 0);
}

/* $Id$ */

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
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>

#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/alternative.h>
#include <avahi-core/core.h>
#include <avahi-core/log.h>

#include "main.h"
#include "simple-protocol.h"
#include "static-services.h"
#include "ini-file-parser.h"

#ifdef HAVE_DBUS
#include "dbus-protocol.h"
#endif

AvahiServer *avahi_server = NULL;

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
    int enable_dbus;
    int fail_on_missing_dbus;
    int drop_root;
    int publish_resolv_conf;
    char ** publish_dns_servers;
    int no_rlimits;
    int debug;

    int rlimit_as_set, rlimit_core_set, rlimit_data_set, rlimit_fsize_set, rlimit_nofile_set, rlimit_stack_set;
    rlim_t rlimit_as, rlimit_core, rlimit_data, rlimit_fsize, rlimit_nofile, rlimit_stack;

#ifdef RLIMIT_NPROC
    int rlimit_nproc_set;
    rlim_t rlimit_nproc;
#endif
} DaemonConfig;

#define RESOLV_CONF "/etc/resolv.conf"

static AvahiSEntryGroup *dns_servers_entry_group = NULL;
static AvahiSEntryGroup *resolv_conf_entry_group = NULL;

static char **resolv_conf = NULL;

static DaemonConfig config;

#define MAX_NAME_SERVERS 10

static int has_prefix(const char *s, const char *prefix) {
    size_t l;

    l = strlen(prefix);
    
    return strlen(s) >= l && strncmp(s, prefix, l) == 0;
}

static int load_resolv_conf(const DaemonConfig *c) {
    int ret = -1;
    FILE *f;
    int i = 0;
    
    avahi_strfreev(resolv_conf);
    resolv_conf = NULL;

    if (!c->publish_resolv_conf)
        return 0;

    if (!(f = fopen(RESOLV_CONF, "r"))) {
        avahi_log_warn("Failed to open "RESOLV_CONF".");
        goto finish;
    }

    resolv_conf = avahi_new0(char*, MAX_NAME_SERVERS+1);

    while (!feof(f) && i < MAX_NAME_SERVERS) {
        char ln[128];
        char *p;

        if (!(fgets(ln, sizeof(ln), f)))
            break;

        ln[strcspn(ln, "\r\n#")] = 0;
        p = ln + strspn(ln, "\t ");

        if (has_prefix(p, "nameserver")) {
            p += 10;
            p += strspn(p, "\t ");
            p[strcspn(p, "\t ")] = 0;
            resolv_conf[i++] = avahi_strdup(p);
        }
    }

    ret = 0;

finish:

    if (ret != 0) {
        avahi_strfreev(resolv_conf);
        resolv_conf = NULL;
    }
        
    if (f)
        fclose(f);

    return ret;
}

static AvahiSEntryGroup* add_dns_servers(AvahiServer *s, AvahiSEntryGroup* g, char **l) {
    char **p;

    assert(s);
    assert(l);

    if (!g) 
        g = avahi_s_entry_group_new(s, NULL, NULL);

    assert(avahi_s_entry_group_is_empty(g));

    for (p = l; *p; p++) {
        AvahiAddress a;
        
        if (!avahi_address_parse(*p, AF_UNSPEC, &a))
            avahi_log_warn("Failed to parse address '%s', ignoring.", *p);
        else
            if (avahi_server_add_dns_server_address(s, g, -1, AF_UNSPEC, NULL, AVAHI_DNS_SERVER_RESOLVE, &a, 53) < 0) {
                avahi_s_entry_group_free(g);
                avahi_log_error("Failed to add DNS server address: %s", avahi_strerror(avahi_server_errno(s)));
                return NULL;
            }
    }

    avahi_s_entry_group_commit(g);

    return g;
}

static void remove_dns_server_entry_groups(void) {

    if (resolv_conf_entry_group)
        avahi_s_entry_group_reset(resolv_conf_entry_group);
    
    if (dns_servers_entry_group) 
        avahi_s_entry_group_reset(dns_servers_entry_group);
}

static void server_callback(AvahiServer *s, AvahiServerState state, void *userdata) {
    DaemonConfig *c = userdata;
    
    assert(s);
    assert(c);

    /** This function is possibly called before the global variable
     * avahi_server has been set, therefore we do it explicitly */

    avahi_server = s;
    
#ifdef HAVE_DBUS
    if (c->enable_dbus)
        dbus_protocol_server_state_changed(state);
#endif

    if (state == AVAHI_SERVER_RUNNING) {
        avahi_log_info("Server startup complete.  Host name is <%s>", avahi_server_get_host_name_fqdn(s));
        static_service_add_to_server();

        remove_dns_server_entry_groups();

        if (resolv_conf && resolv_conf[0])
            resolv_conf_entry_group = add_dns_servers(s, resolv_conf_entry_group, resolv_conf);

        if (c->publish_dns_servers && c->publish_dns_servers[0])
            dns_servers_entry_group = add_dns_servers(s, dns_servers_entry_group, c->publish_dns_servers);

        simple_protocol_restart_queries();
        
    } else if (state == AVAHI_SERVER_COLLISION) {
        char *n;

        static_service_remove_from_server();

        remove_dns_server_entry_groups();

        n = avahi_alternative_host_name(avahi_server_get_host_name(s));
        avahi_log_warn("Host name conflict, retrying with <%s>", n);
        avahi_server_set_host_name(s, n);
        avahi_free(n);
    }
}

static void help(FILE *f, const char *argv0) {
    fprintf(f,
            "%s [options]\n"
            "    -h --help          Show this help\n"
            "    -D --daemonize     Daemonize after startup (implies -s)\n"
            "    -s --syslog        Write log messages to syslog(3) instead of STDERR\n"
            "    -k --kill          Kill a running daemon\n"
            "    -r --reload        Request a running daemon to reload static services\n"
            "    -c --check         Return 0 if a daemon is already running\n"
            "    -V --version       Show version\n"
            "    -f --file=FILE     Load the specified configuration file instead of\n"
            "                       "AVAHI_CONFIG_FILE"\n"
            "       --no-rlimits    Don't enforce resource limits\n"
            "       --no-drop-root  Don't drop privileges\n"
            "       --debug         Increase verbosity\n",
            argv0);
}


static int parse_command_line(DaemonConfig *c, int argc, char *argv[]) {
    int o;

    enum {
        OPTION_NO_RLIMITS = 256,
        OPTION_NO_DROP_ROOT,
        OPTION_DEBUG
    };
    
    static const struct option long_options[] = {
        { "help",         no_argument,       NULL, 'h' },
        { "daemonize",    no_argument,       NULL, 'D' },
        { "kill",         no_argument,       NULL, 'k' },
        { "version",      no_argument,       NULL, 'V' },
        { "file",         required_argument, NULL, 'f' },
        { "reload",       no_argument,       NULL, 'r' },
        { "check",        no_argument,       NULL, 'c' },
        { "syslog",       no_argument,       NULL, 's' },
        { "no-rlimits",   no_argument,       NULL, OPTION_NO_RLIMITS },
        { "no-drop-root", no_argument,       NULL, OPTION_NO_DROP_ROOT },
        { "debug",        no_argument,       NULL, OPTION_DEBUG },
        { NULL, 0, NULL, 0 }
    };

    assert(c);

    opterr = 0;
    while ((o = getopt_long(argc, argv, "hDkVf:rcs", long_options, NULL)) >= 0) {

        switch(o) {
            case 's':
                c->use_syslog = 1;
                break;
            case 'h':
                c->command = DAEMON_HELP;
                break;
            case 'D':
                c->daemonize = 1;
                break;
            case 'k':
                c->command = DAEMON_KILL;
                break;
            case 'V':
                c->command = DAEMON_VERSION;
                break;
            case 'f':
                avahi_free(c->config_file);
                c->config_file = avahi_strdup(optarg);
                break;
            case 'r':
                c->command = DAEMON_RELOAD;
                break;
            case 'c':
                c->command = DAEMON_CHECK;
                break;
            case OPTION_NO_RLIMITS:
                c->no_rlimits = 1;
                break;
            case OPTION_NO_DROP_ROOT:
                c->drop_root = 0;
                break;
            case OPTION_DEBUG:
                c->debug = 1;
                break;
            default:
                fprintf(stderr, "Invalid command line argument: %c\n", o);
                return -1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Too many arguments\n");
        return -1;
    }
        
    return 0;
}

static int is_yes(const char *s) {
    assert(s);
    
    return *s == 'y' || *s == 'Y';
}

static int load_config_file(DaemonConfig *c) {
    int r = -1;
    AvahiIniFile *f;
    AvahiIniFileGroup *g;

    assert(c);

    if (!(f = avahi_ini_file_load(c->config_file ? c->config_file : AVAHI_CONFIG_FILE)))
        goto finish;
    
    for (g = f->groups; g; g = g->groups_next) {
        
        if (strcasecmp(g->name, "server") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {

                if (strcasecmp(p->key, "host-name") == 0) {
                    avahi_free(c->server_config.host_name);
                    c->server_config.host_name = avahi_strdup(p->value);
                } else if (strcasecmp(p->key, "domain-name") == 0) {
                    avahi_free(c->server_config.domain_name);
                    c->server_config.domain_name = avahi_strdup(p->value);
                } else if (strcasecmp(p->key, "use-ipv4") == 0)
                    c->server_config.use_ipv4 = is_yes(p->value);
                else if (strcasecmp(p->key, "use-ipv6") == 0)
                    c->server_config.use_ipv6 = is_yes(p->value);
                else if (strcasecmp(p->key, "check-response-ttl") == 0)
                    c->server_config.check_response_ttl = is_yes(p->value);
                else if (strcasecmp(p->key, "use-iff-running") == 0)
                    c->server_config.use_iff_running = is_yes(p->value);
                else if (strcasecmp(p->key, "enable-dbus") == 0) {

                    if (*(p->value) == 'w' || *(p->value) == 'W') {
                        c->fail_on_missing_dbus = 0;
                        c->enable_dbus = 1;
                    } else if (*(p->value) == 'y' || *(p->value) == 'Y') {
                        c->fail_on_missing_dbus = 1;
                        c->enable_dbus = 1;
                    } else {
                        c->enable_dbus = 0;
                    }
                }
                else if (strcasecmp(p->key, "drop-root") == 0)
                    c->drop_root = is_yes(p->value);
                else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }
            
        } else if (strcasecmp(g->name, "publish") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {
                
                if (strcasecmp(p->key, "publish-addresses") == 0)
                    c->server_config.publish_addresses = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-hinfo") == 0)
                    c->server_config.publish_hinfo = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-workstation") == 0)
                    c->server_config.publish_workstation = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-domain") == 0)
                    c->server_config.publish_domain = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-resolv-conf-dns-servers") == 0)
                    c->publish_resolv_conf = is_yes(p->value);
                else if (strcasecmp(p->key, "publish-dns-servers") == 0) {
                    avahi_strfreev(c->publish_dns_servers);
                    c->publish_dns_servers = avahi_split_csv(p->value);
                } else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }

        } else if (strcasecmp(g->name, "reflector") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {
                
                if (strcasecmp(p->key, "enable-reflector") == 0)
                    c->server_config.enable_reflector = is_yes(p->value);
                else if (strcasecmp(p->key, "reflect-ipv") == 0)
                    c->server_config.reflect_ipv = is_yes(p->value);
                else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }
            }
            
        } else if (strcasecmp(g->name, "rlimits") == 0) {
            AvahiIniFilePair *p;

            for (p = g->pairs; p; p = p->pairs_next) {
                
                if (strcasecmp(p->key, "rlimit-as") == 0) {
                    c->rlimit_as_set = 1;
                    c->rlimit_as = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-core") == 0) {
                    c->rlimit_core_set = 1;
                    c->rlimit_core = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-data") == 0) {
                    c->rlimit_data_set = 1;
                    c->rlimit_data = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-fsize") == 0) {
                    c->rlimit_fsize_set = 1;
                    c->rlimit_fsize = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-nofile") == 0) {
                    c->rlimit_nofile_set = 1;
                    c->rlimit_nofile = atoi(p->value);
                } else if (strcasecmp(p->key, "rlimit-stack") == 0) {
                    c->rlimit_stack_set = 1;
                    c->rlimit_stack = atoi(p->value);
#ifdef RLIMIT_NPROC
                } else if (strcasecmp(p->key, "rlimit-nproc") == 0) {
                    c->rlimit_nproc_set = 1;
                    c->rlimit_nproc = atoi(p->value);
#endif
                } else {
                    avahi_log_error("Invalid configuration key \"%s\" in group \"%s\"\n", p->key, g->name);
                    goto finish;
                }

            }
            
        } else {
            avahi_log_error("Invalid configuration file group \"%s\".\n", g->name);
            goto finish;
        }
    }

    r = 0;

finish:

    if (f)
        avahi_ini_file_free(f);
    
    return r;
}

static void log_function(AvahiLogLevel level, const char *txt) {

    static const int log_level_map[] = {
        LOG_ERR,
        LOG_WARNING,
        LOG_NOTICE,
        LOG_INFO,
        LOG_DEBUG
    };
    
    assert(level < AVAHI_LOG_LEVEL_MAX);
    assert(txt);

    if (!config.debug && level == AVAHI_LOG_DEBUG)
        return;

    daemon_log(log_level_map[level], "%s", txt);
}

static void dump(const char *text, void* userdata) {
    avahi_log_info("%s", text);
}

static void signal_callback(AvahiWatch *watch, int fd, AvahiWatchEvent event, void *userdata) {
    int sig;
    AvahiSimplePoll *simple_poll_api = userdata;
    const AvahiPoll *poll_api;
    
    assert(watch);
    assert(simple_poll_api);

    poll_api = avahi_simple_poll_get(simple_poll_api);

    if ((sig = daemon_signal_next()) <= 0) {
        avahi_log_error("daemon_signal_next() failed");
        poll_api->watch_free(watch);
        return;
    }

    switch (sig) {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            avahi_log_info(
                "Got %s, quitting.",
                sig == SIGINT ? "SIGINT" :
                (sig == SIGQUIT ? "SIGQUIT" : "SIGTERM"));
            avahi_simple_poll_quit(simple_poll_api);
            break;

        case SIGHUP:
            avahi_log_info("Got SIGHUP, reloading.");
            static_service_load();
            static_service_add_to_server();

            if (resolv_conf_entry_group)
                avahi_s_entry_group_reset(resolv_conf_entry_group);

            load_resolv_conf(&config);
            
            if (resolv_conf && resolv_conf[0])
                resolv_conf_entry_group = add_dns_servers(avahi_server, resolv_conf_entry_group, resolv_conf);

            break;

        case SIGUSR1:
            avahi_log_info("Got SIGUSR1, dumping record data.");
            avahi_server_dump(avahi_server, dump, NULL);
            break;

        default:
            avahi_log_warn("Got spurious signal, ignoring.");
            break;
    }
}

static int run_server(DaemonConfig *c) {
    int r = -1;
    int error;
    AvahiSimplePoll *simple_poll_api;
    const AvahiPoll *poll_api;
    AvahiWatch *sig_watch;

    assert(c);

    if (!(simple_poll_api = avahi_simple_poll_new())) {
        avahi_log_error("Failed to create main loop object.");
        goto finish;
    }

    poll_api = avahi_simple_poll_get(simple_poll_api);

    if (daemon_signal_init(SIGINT, SIGQUIT, SIGHUP, SIGTERM, SIGUSR1, 0) < 0) {
        avahi_log_error("Could not register signal handlers (%s).", strerror(errno));
        goto finish;
    }

    if (!(sig_watch = poll_api->watch_new(poll_api, daemon_signal_fd(), AVAHI_WATCH_IN, signal_callback, simple_poll_api))) {
        avahi_log_error( "Failed to create signal watcher");
        goto finish;
    }

    if (simple_protocol_setup(poll_api) < 0)
        goto finish;
    if (c->enable_dbus) {
#ifdef HAVE_DBUS
        if (dbus_protocol_setup(poll_api) < 0) {

            if (c->fail_on_missing_dbus)
                goto finish;

            avahi_log_warn("WARNING: Failed to contact D-BUS daemon, disabling D-BUS support.");
            c->enable_dbus = 0;
        }
#else
        avahi_log_warn("WARNING: We are configured to enable D-BUS but it was not compiled in");
        c->enabled_dbus = 0;
#endif
    }
    
    load_resolv_conf(c);
    static_service_load();

    if (!(avahi_server = avahi_server_new(poll_api, &c->server_config, server_callback, c, &error))) {
        avahi_log_error("Failed to create server: %s", avahi_strerror(error));
        goto finish;
    }


    if (c->daemonize)
        daemon_retval_send(0);

    for (;;) {
        if ((r = avahi_simple_poll_iterate(simple_poll_api, -1)) < 0) {

            /* We handle signals through an FD, so let's continue */
            if (errno == EINTR)
                continue;
            
            avahi_log_error("poll(): %s", strerror(errno));
            goto finish;
        } else if (r > 0)
            /* Quit */
            break;
    }
    

finish:
    
    static_service_remove_from_server();
    static_service_free_all();
    remove_dns_server_entry_groups();
    
    simple_protocol_shutdown();

#ifdef ENABLE_DBUS
    if (c->enable_dbus)
        dbus_protocol_shutdown();
#endif

    if (avahi_server)
        avahi_server_free(avahi_server);

    daemon_signal_done();

    if (sig_watch)
        poll_api->watch_free(sig_watch);

    if (simple_poll_api)
        avahi_simple_poll_free(simple_poll_api);

    if (r != 0 && c->daemonize)
        daemon_retval_send(1);
    
    return r;
}

#define set_env(key, value) putenv(avahi_strdup_printf("%s=%s", (key), (value)))

static int drop_root(void) {
    struct passwd *pw;
    struct group * gr;
    int r;
    
    if (!(pw = getpwnam(AVAHI_USER))) {
        avahi_log_error( "Failed to find user '"AVAHI_USER"'.");
        return -1;
    }

    if (!(gr = getgrnam(AVAHI_GROUP))) {
        avahi_log_error( "Failed to find group '"AVAHI_GROUP"'.");
        return -1;
    }

    avahi_log_info("Found user '"AVAHI_USER"' (UID %lu) and group '"AVAHI_GROUP"' (GID %lu).", (unsigned long) pw->pw_uid, (unsigned long) gr->gr_gid);

    if (initgroups(AVAHI_USER, gr->gr_gid) != 0) {
        avahi_log_error("Failed to change group list: %s", strerror(errno));
        return -1;
    }

#if defined(HAVE_SETRESGID)
    r = setresgid(gr->gr_gid, gr->gr_gid, gr->gr_gid);
#elif defined(HAVE_SETREGID)
    r = setregid(gr->gr_gid, gr->gr_gid);
#else
    if ((r = setgid(gr->gr_gid)) >= 0)
        r = setegid(gr->gr_gid);
#endif

    if (r < 0) {
        avahi_log_error("Failed to change GID: %s", strerror(errno));
        return -1;
    }

#if defined(HAVE_SETRESUID)
    r = setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid);
#elif defined(HAVE_SETREUID)
    r = setreuid(pw->pw_uid, pw->pw_uid);
#else
    if ((r = setuid(pw->pw_uid)) >= 0)
        r = seteuid(pw->pw_uid);
#endif

    if (r < 0) {
        avahi_log_error("Failed to change UID: %s", strerror(errno));
        return -1;
    }

    set_env("USER", pw->pw_name);
    set_env("LOGNAME", pw->pw_name);
    set_env("HOME", pw->pw_dir);
    
    avahi_log_info("Successfully dropped root privileges.");

    return 0;
}

static const char* pid_file_proc(void) {
    return AVAHI_DAEMON_RUNTIME_DIR"/pid";
}

static int make_runtime_dir(void) {
    int r = -1;
    mode_t u;
    int reset_umask = 0;
    struct passwd *pw;
    struct group * gr;
    struct stat st;

    if (!(pw = getpwnam(AVAHI_USER))) {
        avahi_log_error( "Failed to find user '"AVAHI_USER"'.");
        goto fail;
    }

    if (!(gr = getgrnam(AVAHI_GROUP))) {
        avahi_log_error( "Failed to find group '"AVAHI_GROUP"'.");
        goto fail;
    }

    u = umask(0000);
    reset_umask = 1;
    
    if (mkdir(AVAHI_DAEMON_RUNTIME_DIR, 0755) < 0 && errno != EEXIST) {
        avahi_log_error("mkdir(\""AVAHI_DAEMON_RUNTIME_DIR"\"): %s", strerror(errno));
        goto fail;
    }
    
    chown(AVAHI_DAEMON_RUNTIME_DIR, pw->pw_uid, gr->gr_gid);

    if (stat(AVAHI_DAEMON_RUNTIME_DIR, &st) < 0) {
        avahi_log_error("stat(): %s\n", strerror(errno));
        goto fail;
    }

    if (!S_ISDIR(st.st_mode) || st.st_uid != pw->pw_uid || st.st_gid != gr->gr_gid) {
        avahi_log_error("Failed to create runtime directory "AVAHI_DAEMON_RUNTIME_DIR".");
        goto fail;
    }

    r = 0;

fail:
    if (reset_umask)
        umask(u);
    return r;
}

static void set_one_rlimit(int resource, rlim_t limit, const char *name) {
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = limit;

    if (setrlimit(resource, &rl) < 0)
        avahi_log_warn("setrlimit(%s, {%u, %u}) failed: %s", name, (unsigned) limit, (unsigned) limit, strerror(errno));
}

static void enforce_rlimits(void) {

    if (config.rlimit_as_set)
        set_one_rlimit(RLIMIT_AS, config.rlimit_as, "RLIMIT_AS");
    if (config.rlimit_core_set)
        set_one_rlimit(RLIMIT_CORE, config.rlimit_core, "RLIMIT_CORE");
    if (config.rlimit_data_set)
        set_one_rlimit(RLIMIT_DATA, config.rlimit_data, "RLIMIT_DATA");
    if (config.rlimit_fsize_set)
        set_one_rlimit(RLIMIT_FSIZE, config.rlimit_fsize, "RLIMIT_FSIZE");
    if (config.rlimit_nofile_set)
        set_one_rlimit(RLIMIT_NOFILE, config.rlimit_nofile, "RLIMIT_NOFILE");
    if (config.rlimit_stack_set)
        set_one_rlimit(RLIMIT_STACK, config.rlimit_stack, "RLIMIT_STACK");
#ifdef RLIMIT_NPROC
    if (config.rlimit_nproc_set)
        set_one_rlimit(RLIMIT_NPROC, config.rlimit_nproc, "RLIMIT_NPROC");
#endif

#ifdef RLIMIT_MEMLOCK
    /* We don't need locked memory */
    set_one_rlimit(RLIMIT_MEMLOCK, 0, "RLIMIT_MEMLOCK");
#endif
}

#define RANDOM_DEVICE "/dev/urandom"

static void init_rand_seed(void) {
    int fd;
    unsigned seed = 0;

    /* Try to initialize seed from /dev/urandom, to make it a little
     * less predictable, and to make sure that multiple machines
     * booted at the same time choose different random seeds.  */
    if ((fd = open(RANDOM_DEVICE, O_RDONLY)) >= 0) {
        read(fd, &seed, sizeof(seed));
        close(fd);
    }

    /* If the initialization failed by some reason, we add the time to the seed*/
    seed |= (unsigned) time(NULL);

    srand(seed);
}

int main(int argc, char *argv[]) {
    int r = 255;
    const char *argv0;
    int wrote_pid_file = 0;

    avahi_set_log_function(log_function);

    init_rand_seed();
    
    avahi_server_config_init(&config.server_config);
    config.command = DAEMON_RUN;
    config.daemonize = 0;
    config.config_file = NULL;
#ifdef HAVE_DBUS
    config.enable_dbus = 1;
    config.fail_on_missing_dbus = 1;
#else
    config.enable_dbus = 0;
    config.fail_on_missing_dbus = 0;
#endif
    config.drop_root = 1;
    config.publish_dns_servers = NULL;
    config.publish_resolv_conf = 0;
    config.use_syslog = 0;
    config.no_rlimits = 0;
    config.debug = 0;
    
    config.rlimit_as_set = 0;
    config.rlimit_core_set = 0;
    config.rlimit_data_set = 0;
    config.rlimit_fsize_set = 0;
    config.rlimit_nofile_set = 0;
    config.rlimit_stack_set = 0;
#ifdef RLIMIT_NPROC
    config.rlimit_nproc_set = 0;
#endif
    
    if ((argv0 = strrchr(argv[0], '/')))
        argv0++;
    else
        argv0 = argv[0];

    daemon_pid_file_ident = (const char *) argv0;
    daemon_log_ident = (char*) argv0;
    daemon_pid_file_proc = pid_file_proc;
    
    if (parse_command_line(&config, argc, argv) < 0)
        goto finish;

    if (config.command == DAEMON_HELP) {
        help(stdout, argv0);
        r = 0;
    } else if (config.command == DAEMON_VERSION) {
        printf("%s "PACKAGE_VERSION"\n", argv0);
        r = 0;
    } else if (config.command == DAEMON_KILL) {
        if (daemon_pid_file_kill_wait(SIGTERM, 5) < 0) {
            avahi_log_warn("Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }

        r = 0;

    } else if (config.command == DAEMON_RELOAD) {
        if (daemon_pid_file_kill(SIGHUP) < 0) {
            avahi_log_warn("Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }

        r = 0;
        
    } else if (config.command == DAEMON_CHECK)
        r = (daemon_pid_file_is_running() >= 0) ? 0 : 1;
    else if (config.command == DAEMON_RUN) {
        pid_t pid;

        if (getuid() != 0 && config.drop_root) {
            avahi_log_error("This program is intended to be run as root.");
            goto finish;
        }
        
        if ((pid = daemon_pid_file_is_running()) >= 0) {
            avahi_log_error("Daemon already running on PID %u", pid);
            goto finish;
        }

        if (load_config_file(&config) < 0)
            goto finish;
        
        if (config.daemonize) {
            daemon_retval_init();
            
            if ((pid = daemon_fork()) < 0)
                goto finish;
            else if (pid != 0) {
                int ret;
                /** Parent **/

                if ((ret = daemon_retval_wait(20)) < 0) {
                    avahi_log_error("Could not recieve return value from daemon process.");
                    goto finish;
                }

                r = ret;
                goto finish;
            }

            /* Child */
        }

        if (config.use_syslog || config.daemonize)
            daemon_log_use = DAEMON_LOG_SYSLOG;

        if (make_runtime_dir() < 0)
            goto finish;

        if (config.drop_root) {
            if (drop_root() < 0)
                goto finish;
        }

        if (daemon_pid_file_create() < 0) {
            avahi_log_error("Failed to create PID file: %s", strerror(errno));

            if (config.daemonize)
                daemon_retval_send(1);
            goto finish;
        } else
            wrote_pid_file = 1;

        if (!config.no_rlimits)
            enforce_rlimits();

        chdir("/");
        
        avahi_log_info("%s "PACKAGE_VERSION" starting up.", argv0);
        
        if (run_server(&config) == 0)
            r = 0;
    }
        
finish:

    if (config.daemonize)
        daemon_retval_done();

    avahi_server_config_free(&config.server_config);
    avahi_free(config.config_file);
    avahi_strfreev(config.publish_dns_servers);
    avahi_strfreev(resolv_conf);

    if (wrote_pid_file)
        daemon_pid_file_remove();
    
    return r;
}

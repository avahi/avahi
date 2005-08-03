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

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>

#include <avahi-core/core.h>
#include <avahi-core/log.h>

#include "main.h"
#include "simple-protocol.h"
#include "dbus-protocol.h"
#include "static-services.h"

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
    gboolean daemonize;
    gchar *config_file;
    gboolean enable_dbus;
    gboolean drop_root;
    gboolean publish_resolv_conf;
    gchar ** publish_dns_servers;
} DaemonConfig;

#define RESOLV_CONF "/etc/resolv.conf"

static AvahiEntryGroup *dns_servers_entry_group = NULL;
static AvahiEntryGroup *resolv_conf_entry_group = NULL;

static gchar **resolv_conf = NULL;

static DaemonConfig config;

#define MAX_NAME_SERVERS 10

static gint load_resolv_conf(const DaemonConfig *c) {
    gint ret = -1;
    FILE *f;
    gint i = 0;
    
    g_strfreev(resolv_conf);
    resolv_conf = NULL;

    if (!c->publish_resolv_conf)
        return 0;

    if (!(f = fopen(RESOLV_CONF, "r"))) {
        avahi_log_warn("Failed to open "RESOLV_CONF".");
        goto finish;
    }

    resolv_conf = g_new0(gchar*, MAX_NAME_SERVERS+1);

    while (!feof(f) && i < MAX_NAME_SERVERS) {
        char ln[128];
        gchar *p;

        if (!(fgets(ln, sizeof(ln), f)))
            break;

        ln[strcspn(ln, "\r\n#")] = 0;
        p = ln + strspn(ln, "\t ");

        if (g_str_has_prefix(p, "nameserver")) {
            p += 10;
            p += strspn(p, "\t ");
            p[strcspn(p, "\t ")] = 0;
            resolv_conf[i++] = strdup(p);
        }
    }

    ret = 0;

finish:

    if (ret != 0) {
        g_strfreev(resolv_conf);
        resolv_conf = NULL;
    }
        
    if (f)
        fclose(f);

    return ret;
}

static AvahiEntryGroup* add_dns_servers(AvahiServer *s, AvahiEntryGroup* g, gchar **l) {
    gchar **p;

    g_assert(s);
    g_assert(l);

    if (!g) 
        g = avahi_entry_group_new(s, NULL, NULL);

    g_assert(avahi_entry_group_is_empty(g));

    for (p = l; *p; p++) {
        AvahiAddress a;
        
        if (!avahi_address_parse(*p, AF_UNSPEC, &a))
            avahi_log_warn("Failed to parse address '%s', ignoring.", *p);
        else
            if (avahi_server_add_dns_server_address(s, g, -1, AF_UNSPEC, NULL, AVAHI_DNS_SERVER_RESOLVE, &a, 53) < 0) {
                avahi_entry_group_free(g);
                return NULL;
            }
    }

    avahi_entry_group_commit(g);

    return g;
}

static void remove_dns_server_entry_groups(void) {

    if (resolv_conf_entry_group)
        avahi_entry_group_reset(resolv_conf_entry_group);
    
    if (dns_servers_entry_group) 
        avahi_entry_group_reset(dns_servers_entry_group);
}

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {
    DaemonConfig *c = userdata;
    
    g_assert(s);
    g_assert(c);

#ifdef ENABLE_DBUS
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
        gchar *n;

        static_service_remove_from_server();

        remove_dns_server_entry_groups();

        n = avahi_alternative_host_name(avahi_server_get_host_name(s));
        avahi_log_warn("Host name conflict, retrying with <%s>", n);
        avahi_server_set_host_name(s, n);
        g_free(n);
    }
}

static void help(FILE *f, const gchar *argv0) {
    fprintf(f,
            "%s [options]\n"
            "    -h --help        Show this help\n"
            "    -D --daemonize   Daemonize after startup\n"
            "    -k --kill        Kill a running daemon\n"
            "    -r --reload      Request a running daemon to reload static services\n"
            "    -c --check       Return 0 if a daemon is already running\n"
            "    -V --version     Show version\n"
            "    -f --file=FILE   Load the specified configuration file instead of\n"
            "                     "AVAHI_CONFIG_FILE"\n",
            argv0);
}

static gint parse_command_line(DaemonConfig *c, int argc, char *argv[]) {
    gint o;
    
    static const struct option const long_options[] = {
        { "help",      no_argument,       NULL, 'h' },
        { "daemonize", no_argument,       NULL, 'D' },
        { "kill",      no_argument,       NULL, 'k' },
        { "version",   no_argument,       NULL, 'V' },
        { "file",      required_argument, NULL, 'f' },
        { "reload",    no_argument,       NULL, 'r' },
        { "check",     no_argument,       NULL, 'c' },
    };

    g_assert(c);

    opterr = 0;
    while ((o = getopt_long(argc, argv, "hDkVf:rc", long_options, NULL)) >= 0) {

        switch(o) {
            case 'h':
                c->command = DAEMON_HELP;
                break;
            case 'D':
                c->daemonize = TRUE;
                break;
            case 'k':
                c->command = DAEMON_KILL;
                break;
            case 'V':
                c->command = DAEMON_VERSION;
                break;
            case 'f':
                g_free(c->config_file);
                c->config_file = g_strdup(optarg);
                break;
            case 'r':
                c->command = DAEMON_RELOAD;
                break;
            case 'c':
                c->command = DAEMON_CHECK;
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

static gboolean is_yes(const gchar *s) {
    g_assert(s);
    
    return *s == 'y' || *s == 'Y';
}

static gint load_config_file(DaemonConfig *c) {
    int r = -1;
    GKeyFile *f = NULL;
    GError *err = NULL;
    gchar **groups = NULL, **g, **keys = NULL, *v = NULL;

    g_assert(c);
    
    f = g_key_file_new();
    g_key_file_set_list_separator(f, ',');
    
    if (!g_key_file_load_from_file(f, c->config_file ? c->config_file : AVAHI_CONFIG_FILE, G_KEY_FILE_NONE, &err)) {
        fprintf(stderr, "Unable to read config file: %s\n", err->message);
        goto finish;
    }

    groups = g_key_file_get_groups(f, NULL);

    for (g = groups; *g; g++) {
        if (g_strcasecmp(*g, "server") == 0) {
            gchar **k;
            
            keys = g_key_file_get_keys(f, *g, NULL, NULL);

            for (k = keys; *k; k++) {

                v = g_key_file_get_value(f, *g, *k, NULL);
                
                if (g_strcasecmp(*k, "host-name") == 0) {
                    g_free(c->server_config.host_name);
                    c->server_config.host_name = v;
                    v = NULL;
                } else if (g_strcasecmp(*k, "domain-name") == 0) {
                    g_free(c->server_config.domain_name);
                    c->server_config.domain_name = v;
                    v = NULL;
                } else if (g_strcasecmp(*k, "use-ipv4") == 0)
                    c->server_config.use_ipv4 = is_yes(v);
                else if (g_strcasecmp(*k, "use-ipv6") == 0)
                    c->server_config.use_ipv6 = is_yes(v);
                else if (g_strcasecmp(*k, "check-response-ttl") == 0)
                    c->server_config.check_response_ttl = is_yes(v);
                else if (g_strcasecmp(*k, "use-iff-running") == 0)
                    c->server_config.use_iff_running = is_yes(v);
                else if (g_strcasecmp(*k, "enable-dbus") == 0)
                    c->enable_dbus = is_yes(v);
                else if (g_strcasecmp(*k, "drop-root") == 0)
                    c->drop_root = is_yes(v);
                else {
                    fprintf(stderr, "Invalid configuration key \"%s\" in group \"%s\"\n", *k, *g);
                    goto finish;
                }

                g_free(v);
                v = NULL;
            }
        
            g_strfreev(keys);
            keys = NULL;
            
        } else if (g_strcasecmp(*g, "publish") == 0) {
            gchar **k;
            
            keys = g_key_file_get_keys(f, *g, NULL, NULL);

            for (k = keys; *k; k++) {

                v = g_key_file_get_string(f, *g, *k, NULL);
                
                if (g_strcasecmp(*k, "publish-addresses") == 0)
                    c->server_config.publish_addresses = is_yes(v);
                else if (g_strcasecmp(*k, "publish-hinfo") == 0)
                    c->server_config.publish_hinfo = is_yes(v);
                else if (g_strcasecmp(*k, "publish-workstation") == 0)
                    c->server_config.publish_workstation = is_yes(v);
                else if (g_strcasecmp(*k, "publish-domain") == 0)
                    c->server_config.publish_domain = is_yes(v);
                else if (g_strcasecmp(*k, "publish-resolv-conf-dns-servers") == 0)
                    c->publish_resolv_conf = is_yes(v);
                else if (g_strcasecmp(*k, "publish-dns-servers") == 0) {
                    g_strfreev(c->publish_dns_servers);
                    c->publish_dns_servers = g_key_file_get_string_list(f, *g, *k, NULL, NULL);
                } else {
                    fprintf(stderr, "Invalid configuration key \"%s\" in group \"%s\"\n", *k, *g);
                    goto finish;
                }

                g_free(v);
                v = NULL;
            }

            g_strfreev(keys);
            keys = NULL;

        } else if (g_strcasecmp(*g, "reflector") == 0) {
            gchar **k;
            
            keys = g_key_file_get_keys(f, *g, NULL, NULL);

            for (k = keys; *k; k++) {

                v = g_key_file_get_string(f, *g, *k, NULL);
                
                if (g_strcasecmp(*k, "enable-reflector") == 0)
                    c->server_config.enable_reflector = is_yes(v);
                else if (g_strcasecmp(*k, "reflect-ipv") == 0)
                    c->server_config.reflect_ipv = is_yes(v);
                else {
                    fprintf(stderr, "Invalid configuration key \"%s\" in group \"%s\"\n", *k, *g);
                    goto finish;
                }

                g_free(v);
                v = NULL;
            }
    
            g_strfreev(keys);
            keys = NULL;
            
        } else {
            fprintf(stderr, "Invalid configuration file group \"%s\".\n", *g);
            goto finish;
        }
    }

    r = 0;

finish:

    g_strfreev(groups);
    g_strfreev(keys);
    g_free(v);

    if (err)
        g_error_free (err);

    if (f)
        g_key_file_free(f);
    
    return r;
}

static void log_function(AvahiLogLevel level, const gchar *txt) {

    static const int const log_level_map[] = {
        LOG_ERR,
        LOG_WARNING,
        LOG_NOTICE,
        LOG_INFO,
        LOG_DEBUG
    };
    
    g_assert(level < AVAHI_LOG_LEVEL_MAX);
    g_assert(txt);

    daemon_log(log_level_map[level], "%s", txt);
}

static void dump(const gchar *text, gpointer userdata) {
    avahi_log_info("%s", text);
}

static gboolean signal_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    gint sig;
    GMainLoop *loop = data;
    
    g_assert(source);
    g_assert(loop);

    if ((sig = daemon_signal_next()) <= 0) {
        avahi_log_error("daemon_signal_next() failed");
        return FALSE;
    }

    switch (sig) {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            avahi_log_info(
                "Got %s, quitting.",
                sig == SIGINT ? "SIGINT" :
                (sig == SIGQUIT ? "SIGQUIT" : "SIGTERM"));
            g_main_loop_quit(loop);
            break;

        case SIGHUP:
            avahi_log_info("Got SIGHUP, reloading.");
            static_service_load();
            static_service_add_to_server();

            if (resolv_conf_entry_group)
                avahi_entry_group_reset(resolv_conf_entry_group);

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

    return TRUE;
}

static gint run_server(DaemonConfig *c) {
    GMainLoop *loop = NULL;
    gint r = -1;
    GIOChannel *io = NULL;
    guint watch_id = (guint) -1;

    g_assert(c);
    
    loop = g_main_loop_new(NULL, FALSE);

    if (daemon_signal_init(SIGINT, SIGQUIT, SIGHUP, SIGTERM, SIGUSR1, 0) < 0) {
        avahi_log_error("Could not register signal handlers (%s).", strerror(errno));
        goto finish;
    }

    if (!(io = g_io_channel_unix_new(daemon_signal_fd()))) {
        avahi_log_error( "Failed to create signal io channel.");
        goto finish;
    }

    g_io_channel_set_close_on_unref(io, FALSE);
    g_io_add_watch(io, G_IO_IN, signal_callback, loop);
    
    if (simple_protocol_setup(NULL) < 0)
        goto finish;
    
#ifdef ENABLE_DBUS
    if (c->enable_dbus)
        if (dbus_protocol_setup(loop) < 0)
            goto finish;
#endif
    
    if (!(avahi_server = avahi_server_new(NULL, &c->server_config, server_callback, c)))
        goto finish;

    load_resolv_conf(c);
    
    static_service_load();

    if (c->daemonize) {
        daemon_retval_send(0);
        r = 0;
    }

    g_main_loop_run(loop);

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

    if (watch_id != (guint) -1)
        g_source_remove(watch_id);
    
    if (io)
        g_io_channel_unref(io);

        
    if (loop)
        g_main_loop_unref(loop);

    if (r != 0 && c->daemonize)
        daemon_retval_send(1);
    
    return r;
}

static gint drop_root(void) {
    struct passwd *pw;
    struct group * gr;
    gint r;
    
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

    g_setenv("USER", pw->pw_name, 1);
    g_setenv("LOGNAME", pw->pw_name, 1);
    g_setenv("HOME", pw->pw_dir, 1);
    
    avahi_log_info("Successfully dropped root privileges.");

    return 0;
}

static const char* pid_file_proc(void) {
    return AVAHI_DAEMON_RUNTIME_DIR"/pid";
}

static gint make_runtime_dir(void) {
    gint r = -1;
    mode_t u;
    gboolean reset_umask = FALSE;
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
    reset_umask = TRUE;
    
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

int main(int argc, char *argv[]) {
    gint r = 255;
    const gchar *argv0;
    gboolean wrote_pid_file = FALSE;

    avahi_set_log_function(log_function);
    
    avahi_server_config_init(&config.server_config);
    config.command = DAEMON_RUN;
    config.daemonize = FALSE;
    config.config_file = NULL;
    config.enable_dbus = TRUE;
    config.drop_root = TRUE;
    config.publish_dns_servers = NULL;
    config.publish_resolv_conf = FALSE;

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

        if (getuid() != 0) {
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


        printf("%s "PACKAGE_VERSION" starting up.\n", argv0);
        
        chdir("/");

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
            wrote_pid_file = TRUE;

        if (run_server(&config) == 0)
            r = 0;
    }
        
finish:

    if (config.daemonize)
        daemon_retval_done();

    avahi_server_config_free(&config.server_config);
    g_free(config.config_file);
    g_strfreev(config.publish_dns_servers);
    g_strfreev(resolv_conf);

    if (wrote_pid_file)
        daemon_pid_file_remove();
    
    return r;
}

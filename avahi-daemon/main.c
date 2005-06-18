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
    DAEMON_HELP
} DaemonCommand;

typedef struct {
    AvahiServerConfig server_config;
    DaemonCommand command;
    gboolean daemonize;
    gchar *config_file;
    gboolean enable_dbus;
} DaemonConfig;

static void server_callback(AvahiServer *s, AvahiServerState state, gpointer userdata) {
    g_assert(s);

    if (state == AVAHI_SERVER_RUNNING) {
        avahi_log_info("Server startup complete.  Host name is <%s>", avahi_server_get_host_name_fqdn(s));
        static_service_add_to_server();
    } else if (state == AVAHI_SERVER_COLLISION) {
        gchar *n;

        static_service_remove_from_server();
        
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
            "    -V --version     Show version\n"
            "    -f --file=FILE   Load the specified configuration file instead of the default\n",
            argv0);
}

static gint parse_command_line(DaemonConfig *config, int argc, char *argv[]) {
    gint c;
    
    static const struct option const long_options[] = {
        { "help",      no_argument,       NULL, 'h' },
        { "daemonize", no_argument,       NULL, 'D' },
        { "kill",      no_argument,       NULL, 'k' },
        { "version",   no_argument,       NULL, 'V' },
        { "file",      required_argument, NULL, 'f' },
    };

    g_assert(config);

    opterr = 0;
    while ((c = getopt_long(argc, argv, "hDkVf:", long_options, NULL)) >= 0) {

        switch(c) {
            case 'h':
                config->command = DAEMON_HELP;
                break;
            case 'D':
                config->daemonize = TRUE;
                break;
            case 'k':
                config->command = DAEMON_KILL;
                break;
            case 'V':
                config->command = DAEMON_VERSION;
                break;
            case 'f':
                g_free(config->config_file);
                config->config_file = g_strdup(optarg);
                break;
            default:
                fprintf(stderr, "Invalid command line argument: %c\n", c);
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

static gint load_config_file(DaemonConfig *config) {
    int r = -1;
    GKeyFile *f = NULL;
    GError *err = NULL;
    gchar **groups = NULL, **g, **keys = NULL, *v = NULL;

    g_assert(config);
    
    f = g_key_file_new();
    
    if (!g_key_file_load_from_file(f, config->config_file ? config->config_file : AVAHI_CONFIG_FILE, G_KEY_FILE_NONE, &err)) {
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
                    g_free(config->server_config.host_name);
                    config->server_config.host_name = v;
                    v = NULL;
                } else if (g_strcasecmp(*k, "domain-name") == 0) {
                    g_free(config->server_config.domain_name);
                    config->server_config.domain_name = v;
                    v = NULL;
                } else if (g_strcasecmp(*k, "use-ipv4") == 0)
                    config->server_config.use_ipv4 = is_yes(v);
                else if (g_strcasecmp(*k, "use-ipv6") == 0)
                    config->server_config.use_ipv6 = is_yes(v);
                else if (g_strcasecmp(*k, "check-response-ttl") == 0)
                    config->server_config.check_response_ttl = is_yes(v);
                else if (g_strcasecmp(*k, "use-iff-running") == 0)
                    config->server_config.use_iff_running = is_yes(v);
                else if (g_strcasecmp(*k, "enable-dbus") == 0)
                    config->enable_dbus = is_yes(v);
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
                    config->server_config.publish_addresses = is_yes(v);
                else if (g_strcasecmp(*k, "publish-hinfo") == 0)
                    config->server_config.publish_hinfo = is_yes(v);
                else if (g_strcasecmp(*k, "publish-workstation") == 0)
                    config->server_config.publish_workstation = is_yes(v);
                else if (g_strcasecmp(*k, "publish-domain") == 0)
                    config->server_config.publish_domain = is_yes(v);
                else {
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
                    config->server_config.enable_reflector = is_yes(v);
                else if (g_strcasecmp(*k, "reflect-ipv") == 0)
                    config->server_config.reflect_ipv = is_yes(v);
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

static gboolean signal_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    gint sig;
    g_assert(source);

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
            g_main_loop_quit((GMainLoop*) data);
            break;

        case SIGHUP:
            avahi_log_info("Got SIGHUP, reloading.");
            static_service_load();
            break;

        default:
            avahi_log_warn("Got spurious signal, ignoring.");
            break;
    }

    return TRUE;
}

static gint run_server(DaemonConfig *config) {
    GMainLoop *loop = NULL;
    gint r = -1;
    GIOChannel *io = NULL;
    guint watch_id = (guint) -1;

    g_assert(config);
    
    loop = g_main_loop_new(NULL, FALSE);

    if (daemon_signal_init(SIGINT, SIGQUIT, SIGHUP, SIGTERM, 0) < 0) {
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
    if (config->enable_dbus)
        if (dbus_protocol_setup(loop) < 0)
            goto finish;
#endif
    
    if (!(avahi_server = avahi_server_new(NULL, &config->server_config, server_callback, NULL)))
        goto finish;
    
    static_service_load();

    if (config->daemonize) {
        daemon_retval_send(0);
        r = 0;
    }

    g_main_loop_run(loop);

finish:
    
    static_service_remove_from_server();
    static_service_free_all();
    
    simple_protocol_shutdown();

#ifdef ENABLE_DBUS
    if (config->enable_dbus)
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

    if (r != 0 && config->daemonize)
        daemon_retval_send(1);
    
    return r;
}

int main(int argc, char *argv[]) {
    gint r = 255;
    DaemonConfig config;
    const gchar *argv0;
    gboolean wrote_pid_file = FALSE;

    avahi_set_log_function(log_function);
    
    avahi_server_config_init(&config.server_config);
    config.command = DAEMON_RUN;
    config.daemonize = FALSE;
    config.config_file = NULL;
    config.enable_dbus = TRUE;

    if ((argv0 = strrchr(argv[0], '/')))
        argv0++;
    else
        argv0 = argv[0];

    daemon_pid_file_ident = daemon_log_ident = (char *) argv0;
    
    if (parse_command_line(&config, argc, argv) < 0)
        goto finish;

    if (load_config_file(&config) < 0)
        goto finish;

    if (config.command == DAEMON_HELP) {
        help(stdout, argv0);
        r = 0;
    } else if (config.command == DAEMON_VERSION) {
        printf("%s "PACKAGE_VERSION"\n", argv0);

    } else if (config.command == DAEMON_KILL) {
        if (daemon_pid_file_kill_wait(SIGTERM, 5) < 0) {
            avahi_log_warn("Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }

        r = 0;
        
    } else if (config.command == DAEMON_RUN) {
        pid_t pid;
        
        if ((pid = daemon_pid_file_is_running()) >= 0) {
            avahi_log_error("Daemon already running on PID %u", pid);
            goto finish;
        }

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

    if (wrote_pid_file)
        daemon_pid_file_remove();
    
    return r;
}

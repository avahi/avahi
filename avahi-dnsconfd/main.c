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

#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include <sys/wait.h>
#include <getopt.h>

#include <glib.h>

#include <avahi-common/llist.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dexec.h>

#define BROWSE_DNS_SERVERS "BROWSE-DNS-SERVERS\n"

static enum {
    ACKWAIT,
    BROWSING
} state = ACKWAIT;

static gboolean quit = FALSE;

static enum {
    DAEMON_RUN,
    DAEMON_KILL,
    DAEMON_REFRESH,
    DAEMON_VERSION,
    DAEMON_HELP,
    DAEMON_CHECK
} command = DAEMON_RUN;

static gboolean daemonize = FALSE;

typedef struct DNSServerInfo DNSServerInfo;

struct DNSServerInfo {
    gint interface;
    guchar protocol;
    gchar *address;
    AVAHI_LLIST_FIELDS(DNSServerInfo, servers);
};

static AVAHI_LLIST_HEAD(DNSServerInfo, servers);

static void server_info_free(DNSServerInfo *i) {
    g_assert(i);

    g_free(i->address);
    
    AVAHI_LLIST_REMOVE(DNSServerInfo, servers, servers, i);
    g_free(i);
}

static DNSServerInfo* get_server_info(gint interface, guchar protocol, const gchar *address) {
    DNSServerInfo *i;
    g_assert(address);

    for (i = servers; i; i = i->servers_next)
        if (i->interface == interface &&
            i->protocol == protocol &&
            strcmp(i->address, address) == 0)
            return i;

    return NULL;
}

static DNSServerInfo* new_server_info(gint interface, guchar protocol, const gchar *address) {
    DNSServerInfo *i;
    
    g_assert(address);

    i = g_new(DNSServerInfo, 1);
    i->interface = interface;
    i->protocol = protocol;
    i->address = g_strdup(address);

    AVAHI_LLIST_PREPEND(DNSServerInfo, servers, servers, i);
    
    return i;
}

static int set_cloexec(int fd) {
    int n;

    assert(fd >= 0);
    
    if ((n = fcntl(fd, F_GETFD)) < 0)
        return -1;

    if (n & FD_CLOEXEC)
        return 0;

    return fcntl(fd, F_SETFD, n|FD_CLOEXEC);
}

static int open_socket(void) {
    int fd = -1;
    struct sockaddr_un sa;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        daemon_log(LOG_ERR, "socket(): %s", strerror(errno));
        goto fail;
    }

    if (set_cloexec(fd) < 0) {
        daemon_log(LOG_ERR, "fcntl(): %s", strerror(errno));
        goto fail;
    }
    
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, AVAHI_SOCKET, sizeof(sa.sun_path)-1);
    sa.sun_path[sizeof(sa.sun_path)-1] = 0;

    if (connect(fd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
        daemon_log(LOG_ERR, "connect(): %s", strerror(errno));
        goto fail;
    }

    return fd;
    
fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

static ssize_t loop_write(int fd, const void*data, size_t size) {

    ssize_t ret = 0;
    g_assert(fd >= 0 && data && size);

    while (size > 0) {
        ssize_t r;

        if ((r = write(fd, data, size)) < 0)
            return r;

        if (r == 0)
            break;

        ret += r;
        data = (const guint8*) data + r;
        size -= r;
    }

    return ret;
}

static gchar *concat_dns_servers(gint interface) {
    DNSServerInfo *i;
    gchar *r = NULL;
    
    for (i = servers; i; i = i->servers_next)
        if (i->interface == interface || interface <= 0) {
            gchar *t;

            if (!r)
                t = g_strdup(i->address);
            else
                t = g_strdup_printf("%s %s", r, i->address);

            g_free(r);
            r = t;
        }

    return r;
}

static gchar *getifname(gint interface, gchar *name, guint len) {
    int fd = -1;
    gchar *ret = NULL;
    struct ifreq ifr;

    g_assert(interface >= 0);
    
    if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        daemon_log(LOG_ERR, "socket(): %s", strerror(errno));
        goto finish;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = interface;
    
    if (ioctl(fd, SIOCGIFNAME, &ifr) < 0) {
        daemon_log(LOG_ERR, "SIOCGIFNAME: %s\n", strerror(errno));
        goto finish;
    }

    strncpy(name, ifr.ifr_name, len-1);
    name[len-1] = 0;
    ret = name;
    
finish:
    if (fd >= 0)
        close(fd);
    
    return ret;
 }

static void run_script(gboolean new, gint interface, guchar protocol, const gchar *address) {
    gchar *p;
    g_assert(interface > 0);
    gint ret;
    gchar ia[16], pa[16];
    gchar name[IFNAMSIZ+1];

    if (!getifname(interface, name, sizeof(name))) 
        return;
    
    p = concat_dns_servers(interface);
    g_setenv("AVAHI_INTERFACE_DNS_SERVERS", p ? p : "", TRUE);
    g_free(p); 

    p = concat_dns_servers(-1);
    g_setenv("AVAHI_DNS_SERVERS", p ? p : "", TRUE);
    g_free(p); 

    g_setenv("AVAHI_INTERFACE", name, TRUE);
    
    snprintf(ia, sizeof(ia), "%i", interface);
    snprintf(pa, sizeof(pa), "%u", protocol);

    if (daemon_exec("/", &ret, AVAHI_DNSCONF_SCRIPT, AVAHI_DNSCONF_SCRIPT, new ? "+" : "-", address, ia, pa, NULL) < 0)
        daemon_log(LOG_WARNING, "Failed to run script");
    else if (ret != 0)
        daemon_log(LOG_WARNING, "Script returned with non-zero exit code %i", ret);
}

static gint new_line(const gchar *l) {
    g_assert(l);

    if (state == ACKWAIT) {
        if (*l != '+') {
            daemon_log(LOG_ERR, "Avahi command failed: %s", l);
            return -1;
        }

        daemon_log(LOG_INFO, "Successfully connected to Avahi daemon.");
        state = BROWSING;
    } else {
        gint interface;
        guint protocol;
        guint port;
        gchar a[64];
        
        g_assert(state == BROWSING); 

        if (*l != '<' && *l != '>') {
            daemon_log(LOG_ERR, "Avahi sent us an invalid browsing line: %s", l);
            return -1;
        }

        if (sscanf(l+1, "%i %u %64s %u", &interface, &protocol, a, &port) != 4) {
            daemon_log(LOG_ERR, "Failed to parse browsing line: %s", l);
            return -1;
        }

        if (*l == '>') {
            if (port != 53)
                daemon_log(LOG_WARNING, "DNS server with port address != 53 found, ignoring");
            else {
                daemon_log(LOG_INFO, "New DNS Server %s (interface: %i.%u)", a, interface, protocol);
                new_server_info(interface, (guchar) protocol, a);
                run_script(TRUE, interface, (guchar) protocol, a);
            }
        } else {
            DNSServerInfo *i;

            if (port == 53) 
                if ((i = get_server_info(interface, (guchar) protocol, a))) {
                    daemon_log(LOG_INFO, "DNS Server %s removed (interface: %i.%u)", a, interface, protocol);
                    server_info_free(i);
                    run_script(FALSE, interface, (guchar) protocol, a);
                }
        }

    }
    
    return 0;
}

static gint do_connect(void) {
    gint fd = -1;
    
    if ((fd = open_socket()) < 0)
        goto fail;

    if (loop_write(fd, BROWSE_DNS_SERVERS, sizeof(BROWSE_DNS_SERVERS)-1) < 0) {
        daemon_log(LOG_ERR, "write(): %s", strerror(errno));
        goto fail;
    }

    state = ACKWAIT;
    return fd;

fail:
    if (fd >= 0)
        close(fd);
    
    return -1;
}

static void free_dns_server_info_list(void) {
    while (servers) {
        gint interface = servers->interface;
        guchar protocol = servers->protocol;
        gchar *address = g_strdup(servers->address);
        server_info_free(servers);
        
        run_script(FALSE, interface, protocol, address);
        g_free(address);
    }
}

static void help(FILE *f, const gchar *argv0) {
    fprintf(f,
            "%s [options]\n"
            "    -h --help        Show this help\n"
            "    -D --daemonize   Daemonize after startup\n"
            "    -k --kill        Kill a running daemon\n"
            "    -r --refresh     Request a running daemon to refresh DNS server data\n"
            "    -c --check       Return 0 if a daemon is already running\n"
            "    -V --version     Show version\n",
            argv0);
}

static gint parse_command_line(int argc, char *argv[]) {
    gint c;
    
    static const struct option const long_options[] = {
        { "help",      no_argument,       NULL, 'h' },
        { "daemonize", no_argument,       NULL, 'D' },
        { "kill",      no_argument,       NULL, 'k' },
        { "version",   no_argument,       NULL, 'V' },
        { "refresh",   no_argument,       NULL, 'r' },
        { "check",     no_argument,       NULL, 'c' },
    };

    opterr = 0;
    while ((c = getopt_long(argc, argv, "hDkVrc", long_options, NULL)) >= 0) {

        switch(c) {
            case 'h':
                command = DAEMON_HELP;
                break;
            case 'D':
                daemonize = TRUE;
                break;
            case 'k':
                command = DAEMON_KILL;
                break;
            case 'V':
                command = DAEMON_VERSION;
                break;
            case 'r':
                command = DAEMON_REFRESH;
                break;
            case 'c':
                command = DAEMON_CHECK;
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

static int run_daemon(void) {
    gint fd = -1, ret = -1;
    gchar buf[1024];
    size_t buflen = 0;

    AVAHI_LLIST_HEAD_INIT(DNSServerInfo, servers);
    
    daemon_signal_init(SIGINT, SIGTERM, SIGCHLD, SIGHUP, 0);
    
    if ((fd = do_connect()) < 0)
        goto finish;

    if (daemonize)
        daemon_retval_send(0);

    ret = 0;

    while (!quit) {
        fd_set rfds, wfds;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        FD_SET(fd, &rfds);
        FD_SET(daemon_signal_fd(), &rfds);

        for (;;) {
            if (select(fd+1, &rfds, NULL, NULL, NULL) < 0) {
                if (errno == EINTR)
                    continue;

                daemon_log(LOG_ERR, "select(): %s", strerror(errno));
                goto finish;
            }

            break;
        }

        if (FD_ISSET(daemon_signal_fd(), &rfds)) {

            int sig;

            if ((sig = daemon_signal_next()) <= 0) {
                daemon_log(LOG_ERR, "daemon_signal_next() failed");
                goto finish;
            }

            switch(sig) {
                case SIGINT:
                case SIGTERM:
                    daemon_log(LOG_INFO, "Got %s, quitting.", sig == SIGINT ? "SIGINT" : "SIGTERM");
                    ret = 0;
                    goto finish;

                case SIGCHLD:
                    waitpid(-1, NULL, WNOHANG);
                    break;
                    
                case SIGHUP:
                    daemon_log(LOG_INFO, "Refreshing DNS Server list");
                    
                    close(fd);
                    free_dns_server_info_list();
                    
                    if ((fd = do_connect()) < 0)
                        goto finish;
                    
                    break;
            }
            
        } else if (FD_ISSET(fd, &rfds)) {
            ssize_t r;
            gchar *n;

            if ((r = read(fd, buf, sizeof(buf) - buflen - 1)) <= 0) {
                daemon_log(LOG_ERR, "read(): %s", r < 0 ? strerror(errno) : "EOF");
                goto finish;
            }

            buflen += r;
            g_assert(buflen <= sizeof(buf)-1);

            while ((n = memchr(buf, '\n', buflen))) {
                *(n++) = 0;

                if (new_line(buf) < 0)
                    goto finish;

                buflen -= (n - buf);
                memmove(buf, n, buflen);
            }

            if (buflen >= sizeof(buf)-1) {
                /* The incoming line is horribly long */
                buf[sizeof(buf)-1] = 0;
                
                if (new_line(buf) < 0)
                    goto finish;
                
                buflen = 0;
            }
        }
    }
    
finish:

    free_dns_server_info_list();

    if (fd >= 0)
        close(fd);
    
    daemon_signal_done();

    if (ret != 0 && daemonize)
        daemon_retval_send(1);
    
    return ret;
}

static const char* pid_file_proc(void) {
    return AVAHI_RUNTIME_DIR"/avahi-dnsconfd.pid";
}

gint main(gint argc, gchar *argv[]) {
    gchar *argv0;
    gint r = 1;
    gboolean wrote_pid_file = FALSE;

    if ((argv0 = strrchr(argv[0], '/')))
        argv0++;
    else
        argv0 = argv[0];

    daemon_pid_file_ident = daemon_log_ident = argv0;
    daemon_pid_file_proc = pid_file_proc;
    
    if (parse_command_line(argc, argv) < 0)
        goto finish;

    if (command == DAEMON_RUN) {
        pid_t pid;

        if (getuid() != 0) {
            daemon_log(LOG_ERR, "This program is intended to be run as root.");
            goto finish;
        }

        if ((pid = daemon_pid_file_is_running()) >= 0) {
            daemon_log(LOG_ERR, "Daemon already running on PID %u", pid);
            goto finish;
        }

        if (daemonize) {
            daemon_retval_init();
            
            if ((pid = daemon_fork()) < 0)
                goto finish;
            else if (pid != 0) {
                int ret;
                /** Parent **/

                if ((ret = daemon_retval_wait(20)) < 0) {
                    daemon_log(LOG_ERR, "Could not recieve return value from daemon process.");
                    goto finish;
                }

                r = ret;
                goto finish;
            }

            /* Child */
        }

        chdir("/");

        if (daemon_pid_file_create() < 0) {
            daemon_log(LOG_ERR, "Failed to create PID file: %s", strerror(errno));

            if (daemonize)
                daemon_retval_send(1);
            goto finish;
        } else
            wrote_pid_file = TRUE;

        if (run_daemon() < 0)
            goto finish;

        r = 0;
    } else if (command == DAEMON_HELP) {
        help(stdout, argv0);
        
        r = 0;
    } else if (command == DAEMON_VERSION) {
        printf("%s "PACKAGE_VERSION"\n", argv0);
        
        r = 0;
    } else if (command == DAEMON_KILL) {
        if (daemon_pid_file_kill_wait(SIGTERM, 5) < 0) {
            daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }
        
        r = 0;
    } else if (command == DAEMON_REFRESH) {
        if (daemon_pid_file_kill(SIGHUP) < 0) {
            daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));
            goto finish;
        }

        r = 0;
    } else if (command == DAEMON_CHECK)
        r = (daemon_pid_file_is_running() >= 0) ? 0 : 1;


    
finish:

    if (daemonize)
        daemon_retval_done();
    
    if (wrote_pid_file)
        daemon_pid_file_remove();

    return r;
}

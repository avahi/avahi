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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <locale.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <langinfo.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <avahi-common/gccmacro.h>
#include <avahi-common/timeval.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/domain.h>
#include <avahi-common/llist.h>
#include <avahi-common/i18n.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include "sigint.h"

#if defined(HAVE_GDBM)
#include "stdb.h"
#endif

typedef enum {
    COMMAND_HELP,
    COMMAND_VERSION,
    COMMAND_BROWSE_SERVICES,
    COMMAND_BROWSE_ALL_SERVICES,
    COMMAND_BROWSE_DOMAINS
#if defined(HAVE_GDBM)
    , COMMAND_DUMP_STDB
#endif
} Command;

typedef enum {
    COLOR_AUTO,
    COLOR_ALWAYS,
    COLOR_NEVER
} ColorMode;

/* Long-only options */
enum {
    ARG_COLOR = 256
};

typedef struct Config {
    int verbose;
    int terminate_on_all_for_now;
    int terminate_on_cache_exhausted;
    char *domain;
    char *stype;
    int ignore_local;
    Command command;
    int resolve;
    int no_fail;
    int parsable;
    ColorMode color;
#if defined(HAVE_GDBM)
    int no_db_lookup;
#endif
} Config;

typedef struct ServiceInfo ServiceInfo;

struct ServiceInfo {
    AvahiIfIndex interface;
    AvahiProtocol protocol;
    char *name, *type, *domain;

    AvahiServiceResolver *resolver;
    Config *config;

    AVAHI_LLIST_FIELDS(ServiceInfo, info);
};

static AvahiSimplePoll *simple_poll = NULL;
static AvahiClient *client = NULL;
static int n_all_for_now = 0, n_cache_exhausted = 0, n_resolving = 0;
static AvahiStringList *browsed_types = NULL;
static ServiceInfo *services = NULL;
static int n_columns = 80;
static int browsing = 0;

/* Terminal presentation state. "pretty" is only enabled when stdout is
 * a terminal and the parsable format wasn't requested, so the output
 * written to pipes and files stays exactly as before. */
static int use_color = 0;
static int pretty = 0;
static int utf8 = 0;
static int columns_from_env = 0;
static volatile sig_atomic_t window_resized = 0;

static int use_spinner = 0;
static int spinner_visible = 0;
static unsigned spinner_frame = 0;
static int n_found = 0;
static AvahiTimeout *spinner_timeout = NULL;

#define SPINNER_INTERVAL_MSEC 100

#define STYLE_RESET    "\033[0m"
#define STYLE_BOLD     "\033[1m"
#define STYLE_DIM      "\033[2m"
#define STYLE_NEW      "\033[1;32m"
#define STYLE_REMOVED  "\033[1;31m"
#define STYLE_RESOLVED "\033[1;36m"
#define STYLE_TYPE     "\033[35m"
#define STYLE_KEY      "\033[36m"

static const char *style(const char *s) {
    return use_color ? s : "";
}

static const char *event_style(char c) {
    return c == '+' ? STYLE_NEW : c == '-' ? STYLE_REMOVED : STYLE_RESOLVED;
}

static void spinner_clear(void) {
    if (!spinner_visible)
        return;

    fputs("\r\033[K", stderr);
    fflush(stderr);
    spinner_visible = 0;
}

static void spinner_draw(void) {
    static const char * const frames_utf8[] = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏" };
    static const char * const frames_ascii[] = { "|", "/", "-", "\\" };
    const char *frame;

    if (utf8)
        frame = frames_utf8[spinner_frame % (sizeof(frames_utf8) / sizeof(frames_utf8[0]))];
    else
        frame = frames_ascii[spinner_frame % (sizeof(frames_ascii) / sizeof(frames_ascii[0]))];

    fprintf(stderr, "\r\033[K%s%s%s ", style(STYLE_RESOLVED), frame, style(STYLE_RESET));
    fprintf(stderr, _("Browsing... %i found"), n_found);
    fflush(stderr);
    spinner_visible = 1;
}

static void spinner_callback(AvahiTimeout *t, AVAHI_GCC_UNUSED void *userdata) {
    struct timeval tv;

    spinner_frame++;
    spinner_draw();
    avahi_simple_poll_get(simple_poll)->timeout_update(t, avahi_elapse_time(&tv, SPINNER_INTERVAL_MSEC, 0));
}

static void spinner_start(void) {
    struct timeval tv;

    if (!use_spinner || spinner_timeout)
        return;

    spinner_timeout = avahi_simple_poll_get(simple_poll)->timeout_new(
        avahi_simple_poll_get(simple_poll),
        avahi_elapse_time(&tv, SPINNER_INTERVAL_MSEC, 0),
        spinner_callback,
        NULL);
}

static void spinner_stop(void) {
    spinner_clear();

    if (spinner_timeout) {
        avahi_simple_poll_get(simple_poll)->timeout_free(spinner_timeout);
        spinner_timeout = NULL;
    }

    /* Don't come back after the first complete list has been shown */
    use_spinner = 0;
}

/* All diagnostics go through here so that they never end up glued to
 * the spinner line */
static void eprintf(const char *format, ...) AVAHI_GCC_PRINTF_ATTR12;

static void eprintf(const char *format, ...) {
    va_list ap;

    spinner_clear();

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

static void update_columns(void) {
    const char *e;
    int n = 80;

    if ((e = getenv("COLUMNS"))) {
        n = atoi(e);
        columns_from_env = 1;
    }
#ifdef TIOCGWINSZ
    else if (isatty(STDOUT_FILENO)) {
        struct winsize ws;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
            n = ws.ws_col;
    }
#endif

    if (n < 40)
        n = 40;

    n_columns = n;
}

static void sigwinch_handler(AVAHI_GCC_UNUSED int sig) {
    window_resized = 1;
}

static void check_resize(void) {
    if (!window_resized)
        return;

    window_resized = 0;
    update_columns();
}

/* Width of the "Type" column. In pretty mode the whole line is made to
 * fit in the terminal: "E Ifce Prot " (14) + name + " " + type + " local"
 * (6), leaving the last column free. The type column keeps its 20 cells
 * as long as the name column can have at least 10, and shrinks with it
 * on narrower terminals. */
static int type_width(void) {
    int avail;

    if (!pretty)
        return 20;

    avail = n_columns - 22;

    if (avail >= 30)
        return 20;

    return avail * 2 / 5;
}

/* Width of the "Name" column */
static int name_width(void) {
    if (!pretty)
        return n_columns - 35;

    return n_columns - 22 - type_width();
}

/* Decodes the next character of s. Returns 1 if it is printable,
 * storing its length in bytes and its width in terminal cells. Invalid
 * or non-printable characters count as a single byte/cell. */
static int next_char(const char *s, size_t n, size_t *len, int *width) {
    mbstate_t state;
    wchar_t wc;
    size_t r;

    memset(&state, 0, sizeof(state));
    r = mbrtowc(&wc, s, n, &state);

    if (r == (size_t) -1 || r == (size_t) -2 || r == 0) {
        *len = 1;
        *width = 1;
        return 0;
    }

    *len = r;

    if (!iswprint((wint_t) wc) || (*width = wcwidth(wc)) < 0) {
        *width = 1;
        return 0;
    }

    return 1;
}

static int display_width(const char *s) {
    size_t n = strlen(s), len;
    int w, total = 0;

    while (n > 0) {
        next_char(s, n, &len, &w);
        total += w;
        s += len;
        n -= len;
    }

    return total;
}

/* Prints s left aligned in a cell of the given width, measured in
 * terminal cells rather than bytes. If truncate is set, text that
 * doesn't fit is cut and an ellipsis is appended. */
static void print_cell(FILE *f, const char *s, int width, int truncate) {
    int used;

    if (truncate && (used = display_width(s)) > width) {
        const char *ellipsis = utf8 ? "…" : "...";
        int ew = utf8 ? 1 : 3, w;
        size_t n = strlen(s), len;

        used = 0;
        while (n > 0) {
            next_char(s, n, &len, &w);

            if (used + w > width - ew)
                break;

            fwrite(s, 1, len, f);
            used += w;
            s += len;
            n -= len;
        }

        fputs(ellipsis, f);
        used += ew;
    } else {
        fputs(s, f);
        used = display_width(s);
    }

    for (; used < width; used++)
        fputc(' ', f);
}

/* Prints raw TXT record data, keeping printable (multibyte) characters
 * and escaping everything else as \ddd like avahi_string_list_to_string() */
static void print_escaped(FILE *f, const char *s, size_t n) {
    size_t len;
    int w;

    while (n > 0) {
        if (*s != '\\' && next_char(s, n, &len, &w))
            fwrite(s, 1, len, f);
        else {
            len = 1;

            if (*s == '\\')
                fputs("\\\\", f);
            else
                fprintf(f, "\\%03u", (unsigned) (uint8_t) *s);
        }

        s += len;
        n -= len;
    }
}

static void check_terminate(Config *c) {

    assert(n_all_for_now >= 0);
    assert(n_cache_exhausted >= 0);
    assert(n_resolving >= 0);

    if (n_all_for_now <= 0 && n_resolving <= 0) {

        spinner_stop();

        if (c->verbose && !c->parsable) {
            printf(_(": All for now\n"));
            n_all_for_now++; /* Make sure that this event is not repeated */
        }

        if (c->terminate_on_all_for_now)
            avahi_simple_poll_quit(simple_poll);
    }

    if (n_cache_exhausted <= 0 && n_resolving <= 0) {

        if (c->verbose && !c->parsable) {
            spinner_clear();
            printf(_(": Cache exhausted\n"));
            n_cache_exhausted++; /* Make sure that this event is not repeated */
        }

        if (c->terminate_on_cache_exhausted)
            avahi_simple_poll_quit(simple_poll);
    }
}

static ServiceInfo *find_service(AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain) {
    ServiceInfo *i;

    for (i = services; i; i = i->info_next)
        if (i->interface == interface &&
            i->protocol == protocol &&
            strcasecmp(i->name, name) == 0 &&
            avahi_domain_equal(i->type, type) &&
            avahi_domain_equal(i->domain, domain))

            return i;

    return NULL;
}

/* Replaces non-printable characters by '_'. Multibyte characters that
 * are printable in the current locale (e.g. UTF-8 names such as
 * "Café") are kept as they are. The result is never longer than from. */
static char *make_printable(const char *from, char *to) {
    size_t n = strlen(from), len;
    char *t = to;
    int w;

    while (n > 0) {
        if (next_char(from, n, &len, &w)) {
            memcpy(t, from, len);
            t += len;
        } else
            *(t++) = '_';

        from += len;
        n -= len;
    }

    *t = 0;

    return to;
}

static void print_service_line(Config *config, char c, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain, int nl) {
    char ifname[IF_NAMESIZE];

#if defined(HAVE_GDBM)
    if (!config->no_db_lookup)
        type = stdb_lookup(type);
#endif

    if (config->parsable) {
        char sn[AVAHI_DOMAIN_NAME_MAX], *e = sn;
        size_t l = sizeof(sn);

        printf("%c;%s;%s;%s;%s;%s%s",
               c,
               interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : _("n/a"),
               protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : _("n/a"),
               avahi_escape_label(name, strlen(name), &e, &l), type, domain, nl ? "\n" : "");

    } else {
        char label[AVAHI_LABEL_MAX];
        make_printable(name, label);

        check_resize();
        spinner_clear();

        printf("%s%c%s %s%6s %4s%s ",
               style(event_style(c)), c, style(STYLE_RESET),
               style(STYLE_DIM),
               interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : _("n/a"),
               protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : _("n/a"),
               style(STYLE_RESET));

        fputs(style(c == '-' ? STYLE_DIM : STYLE_BOLD), stdout);
        print_cell(stdout, label, name_width(), pretty);
        printf("%s %s", style(STYLE_RESET), style(STYLE_TYPE));
        print_cell(stdout, type, type_width(), pretty);
        printf("%s %s%s%s\n", style(STYLE_RESET), style(STYLE_DIM), domain, style(STYLE_RESET));
    }

    fflush(stdout);
}

static void print_card_label(const char *label) {
    printf("   %s%s%s ", style(STYLE_DIM), utf8 ? "│" : "|", style(STYLE_RESET));
    fputs(style(STYLE_DIM), stdout);
    print_cell(stdout, label, 10, 0);
    fputs(style(STYLE_RESET), stdout);
}

/* Human friendly version of the resolver output: one field per line and
 * one TXT record entry per line, split in key and value. */
static void print_resolved_card(const char *host_name, const char *address, uint16_t port, AvahiStringList *txt) {
    AvahiStringList *l;

    print_card_label(_("hostname"));
    printf("%s\n", host_name);
    print_card_label(_("address"));
    printf("%s\n", address);
    print_card_label(_("port"));
    printf("%u\n", port);
    print_card_label(_("txt"));

    if (!txt)
        printf("%s%s%s\n", style(STYLE_DIM), _("(empty)"), style(STYLE_RESET));

    /* The list is stored in reverse order, show the records in the
     * order they were published, like avahi_string_list_to_string() */
    txt = avahi_string_list_reverse(txt);

    for (l = txt; l; l = l->next) {
        const char *text = (const char*) l->text, *eq;
        size_t key_len;

        if (l != txt)
            print_card_label("");

        eq = memchr(text, '=', l->size);
        key_len = eq ? (size_t) (eq - text) : l->size;

        fputs(style(STYLE_KEY), stdout);
        print_escaped(stdout, text, key_len);
        fputs(style(STYLE_RESET), stdout);

        if (eq) {
            printf("%s=%s", style(STYLE_DIM), style(STYLE_RESET));
            print_escaped(stdout, eq + 1, l->size - key_len - 1);
        }

        putchar('\n');
    }

    avahi_string_list_reverse(txt);
}

static void service_resolver_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata) {

    ServiceInfo *i = userdata;

    assert(r);
    assert(i);

    switch (event) {
        case AVAHI_RESOLVER_FOUND: {
            char address[AVAHI_ADDRESS_STR_MAX], *t;

            avahi_address_snprint(address, sizeof(address), a);

            t = avahi_string_list_to_string(txt);

            print_service_line(i->config, '=', interface, protocol, name, type, domain, 0);

            if (i->config->parsable)
                printf(";%s;%s;%u;%s\n",
                       host_name,
                       address,
                       port,
                       t);
            else if (pretty)
                print_resolved_card(host_name, address, port, txt);
            else
                printf("   hostname = [%s]\n"
                       "   address = [%s]\n"
                       "   port = [%u]\n"
                       "   txt = [%s]\n",
                       host_name,
                       address,
                       port,
                       t);

            avahi_free(t);

            break;
        }

        case AVAHI_RESOLVER_FAILURE:

            eprintf(_("Failed to resolve service '%s' of type '%s' in domain '%s': %s\n"), name, type, domain, avahi_strerror(avahi_client_errno(client)));
            break;
    }


    avahi_service_resolver_free(i->resolver);
    i->resolver = NULL;

    assert(n_resolving > 0);
    n_resolving--;
    check_terminate(i->config);
    fflush(stdout);
}

static ServiceInfo *add_service(Config *c, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain) {
    ServiceInfo *i;

    i = avahi_new(ServiceInfo, 1);

    if (c->resolve) {
        if (!(i->resolver = avahi_service_resolver_new(client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, service_resolver_callback, i))) {
            avahi_free(i);
            eprintf(_("Failed to resolve service '%s' of type '%s' in domain '%s': %s\n"), name, type, domain, avahi_strerror(avahi_client_errno(client)));
            return NULL;
        }

        n_resolving++;
    } else
        i->resolver = NULL;

    i->interface = interface;
    i->protocol = protocol;
    i->name = avahi_strdup(name);
    i->type = avahi_strdup(type);
    i->domain = avahi_strdup(domain);
    i->config = c;

    AVAHI_LLIST_PREPEND(ServiceInfo, info, services, i);

    return i;
}

static void remove_service(Config *c, ServiceInfo *i) {
    assert(c);
    assert(i);

    AVAHI_LLIST_REMOVE(ServiceInfo, info, services, i);

    if (i->resolver)
        avahi_service_resolver_free(i->resolver);

    avahi_free(i->name);
    avahi_free(i->type);
    avahi_free(i->domain);
    avahi_free(i);
}

static void service_browser_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags flags,
    void *userdata) {

    Config *c = userdata;

    assert(b);
    assert(c);

    switch (event) {
        case AVAHI_BROWSER_NEW: {
            if (c->ignore_local && (flags & AVAHI_LOOKUP_RESULT_LOCAL))
                break;

            if (find_service(interface, protocol, name, type, domain))
                return;

            add_service(c, interface, protocol, name, type, domain);
            n_found++;

            print_service_line(c, '+', interface, protocol, name, type, domain, 1);
            break;

        }

        case AVAHI_BROWSER_REMOVE: {
            ServiceInfo *info;

            if (!(info = find_service(interface, protocol, name, type, domain)))
                return;

            remove_service(c, info);
            n_found--;

            print_service_line(c, '-', interface, protocol, name, type, domain, 1);
            break;
        }

        case AVAHI_BROWSER_FAILURE:
            eprintf(_("service_browser failed: %s\n"), avahi_strerror(avahi_client_errno(client)));
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            n_cache_exhausted --;
            check_terminate(c);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
            n_all_for_now --;
            check_terminate(c);
            break;
    }
}

static void browse_service_type(Config *c, const char *stype, const char *domain) {
    AvahiServiceBrowser *b;
    AvahiStringList *i;

    assert(c);
    assert(client);
    assert(stype);

    for (i = browsed_types; i; i = i->next)
        if (avahi_domain_equal(stype, (char*) i->text))
            return;

    if (!(b = avahi_service_browser_new(
              client,
              AVAHI_IF_UNSPEC,
              AVAHI_PROTO_UNSPEC,
              stype,
              domain,
              0,
              service_browser_callback,
              c))) {

        eprintf(_("avahi_service_browser_new() failed: %s\n"), avahi_strerror(avahi_client_errno(client)));
        avahi_simple_poll_quit(simple_poll);
    }

    browsed_types = avahi_string_list_add(browsed_types, stype);

    n_all_for_now++;
    n_cache_exhausted++;
}

static void service_type_browser_callback(
    AvahiServiceTypeBrowser *b,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata) {

    Config *c = userdata;

    assert(b);
    assert(c);

    switch (event) {

        case AVAHI_BROWSER_NEW:
            browse_service_type(c, type, domain);
            break;

        case AVAHI_BROWSER_REMOVE:
            /* We're dirty and never remove the browser again */
            break;

        case AVAHI_BROWSER_FAILURE:
            eprintf(_("service_type_browser failed: %s\n"), avahi_strerror(avahi_client_errno(client)));
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            n_cache_exhausted --;
            check_terminate(c);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
            n_all_for_now --;
            check_terminate(c);
            break;
    }
}

static void browse_all(Config *c) {
    AvahiServiceTypeBrowser *b;

    assert(c);

    if (!(b = avahi_service_type_browser_new(
              client,
              AVAHI_IF_UNSPEC,
              AVAHI_PROTO_UNSPEC,
              c->domain,
              0,
              service_type_browser_callback,
              c))) {

        eprintf(_("avahi_service_type_browser_new() failed: %s\n"), avahi_strerror(avahi_client_errno(client)));
        avahi_simple_poll_quit(simple_poll);
    }

    n_cache_exhausted++;
    n_all_for_now++;
}

static void domain_browser_callback(
    AvahiDomainBrowser *b,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata) {

    Config *c = userdata;

    assert(b);
    assert(c);

    switch (event) {

        case AVAHI_BROWSER_NEW:
        case AVAHI_BROWSER_REMOVE: {
            char ifname[IF_NAMESIZE];
            char e = event == AVAHI_BROWSER_NEW ? '+' : '-';

            n_found += e == '+' ? 1 : -1;

            if (c->parsable)
                printf("%c;%s;%s;%s\n",
                       event == AVAHI_BROWSER_NEW ? '+' : '-',
                       interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : "",
                       protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : "",
                       domain);
            else {
                spinner_clear();
                printf("%s%c%s %s%4s %4s%s %s\n",
                       style(event_style(e)), e, style(STYLE_RESET),
                       style(STYLE_DIM),
                       interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : "n/a",
                       protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : "n/a",
                       style(STYLE_RESET),
                       domain);
            }

            fflush(stdout);
            break;
        }

        case AVAHI_BROWSER_FAILURE:
            eprintf(_("domain_browser failed: %s\n"), avahi_strerror(avahi_client_errno(client)));
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            n_cache_exhausted --;
            check_terminate(c);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
            n_all_for_now --;
            check_terminate(c);
            break;
    }
}

static void browse_domains(Config *c) {
    AvahiDomainBrowser *b;

    assert(c);

    if (!(b = avahi_domain_browser_new(
              client,
              AVAHI_IF_UNSPEC,
              AVAHI_PROTO_UNSPEC,
              c->domain,
              AVAHI_DOMAIN_BROWSER_BROWSE,
              0,
              domain_browser_callback,
              c))) {

        eprintf(_("avahi_domain_browser_new() failed: %s\n"), avahi_strerror(avahi_client_errno(client)));
        avahi_simple_poll_quit(simple_poll);
    }

    n_cache_exhausted++;
    n_all_for_now++;
}

static int start(Config *config) {

    assert(!browsing);

    if (config->verbose && !config->parsable) {
        const char *version, *hn;

        if (!(version = avahi_client_get_version_string(client))) {
            eprintf(_("Failed to query version string: %s\n"), avahi_strerror(avahi_client_errno(client)));
            return -1;
        }

        if (!(hn = avahi_client_get_host_name_fqdn(client))) {
            eprintf(_("Failed to query host name: %s\n"), avahi_strerror(avahi_client_errno(client)));
            return -1;
        }

        eprintf(_("Server version: %s; Host name: %s\n"), version, hn);

        check_resize();
        fputs(style(STYLE_BOLD), stderr);

        if (config->command == COMMAND_BROWSE_DOMAINS) {
            /* Translators: This is a column heading with abbreviations for
             *   Event (+/-), Network Interface, Protocol (IPv4/v6), Domain */
            eprintf(_("E Ifce Prot Domain\n"));
        } else {
            /* Translators: This is a column heading with abbreviations for
             *   Event (+/-), Network Interface, Protocol (IPv4/v6), Domain */
            eprintf(_("E Ifce Prot %-*s %-20s Domain\n"), name_width(), _("Name"), _("Type"));
        }

        fputs(style(STYLE_RESET), stderr);
    }

    spinner_start();

    if (config->command == COMMAND_BROWSE_SERVICES)
        browse_service_type(config, config->stype, config->domain);
    else if (config->command == COMMAND_BROWSE_ALL_SERVICES)
        browse_all(config);
    else {
        assert(config->command == COMMAND_BROWSE_DOMAINS);
        browse_domains(config);
    }

    browsing = 1;
    return 0;
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    Config *config = userdata;

    /* This function might be called when avahi_client_new() has not
     * returned yet.*/
    client = c;

    switch (state) {
        case AVAHI_CLIENT_FAILURE:

            if (config->no_fail && avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) {
                int error;

                /* We have been disconnected, so let reconnect */

                eprintf(_("Disconnected, reconnecting ...\n"));

                while (services)
                    remove_service(config, services);

                avahi_client_free(client);
                client = NULL;

                avahi_string_list_free(browsed_types);
                browsed_types = NULL;

                browsing = 0;

                if (!(client = avahi_client_new(avahi_simple_poll_get(simple_poll), AVAHI_CLIENT_NO_FAIL, client_callback, config, &error))) {
                    eprintf(_("Failed to create client object: %s\n"), avahi_strerror(error));
                    avahi_simple_poll_quit(simple_poll);
                }

            } else {
                eprintf(_("Client failure, exiting: %s\n"), avahi_strerror(avahi_client_errno(c)));
                avahi_simple_poll_quit(simple_poll);
            }

            break;

        case AVAHI_CLIENT_S_REGISTERING:
        case AVAHI_CLIENT_S_RUNNING:
        case AVAHI_CLIENT_S_COLLISION:

            if (!browsing)
                if (start(config) < 0)
                    avahi_simple_poll_quit(simple_poll);

            break;

        case AVAHI_CLIENT_CONNECTING:

            if (config->verbose && !config->parsable)
                eprintf(_("Waiting for daemon ...\n"));

            break;
    }
}

static void help(FILE *f, const char *argv0) {
    if (strstr(argv0, "domain"))
        fprintf(f, "%s [options] \n\n", argv0);
    else
        fprintf(f,
                "%s [options] <service type>\n"
                "%s [options] -a\n"
                "%s [options] -D\n"
#if defined(HAVE_GDBM)
                "%s [options] -b\n"
#endif
                "\n",
#if defined(HAVE_GDBM)
                argv0,
#endif
                argv0, argv0, argv0);

    fprintf(f, "%s%s",
            _("    -h --help            Show this help\n"
              "    -V --version         Show version\n"
              "    -D --browse-domains  Browse for browsing domains instead of services\n"
              "    -a --all             Show all services, regardless of the type\n"
              "    -d --domain=DOMAIN   The domain to browse in\n"
              "    -v --verbose         Enable verbose mode\n"
              "    -t --terminate       Terminate after dumping a more or less complete list\n"
              "    -c --cache           Terminate after dumping all entries from the cache\n"
              "    -l --ignore-local    Ignore local services\n"
              "    -r --resolve         Resolve services found\n"
              "    -f --no-fail         Don't fail if the daemon is not available\n"
              "    -p --parsable        Output in parsable format\n"),
#if defined(HAVE_GDBM)
            _("    -k --no-db-lookup    Don't lookup service types\n"
              "    -b --dump-db         Dump service type database\n")
#else
            ""
#endif
            );

    fprintf(f, "%s",
            _("       --color[=WHEN]   Colorize the output: auto (default), always or never\n"));
}

static int parse_command_line(Config *c, const char *argv0, int argc, char *argv[]) {
    int o;

    static const struct option long_options[] = {
        { "help",           no_argument,       NULL, 'h' },
        { "version",        no_argument,       NULL, 'V' },
        { "browse-domains", no_argument,       NULL, 'D' },
        { "domain",         required_argument, NULL, 'd' },
        { "all",            no_argument,       NULL, 'a' },
        { "verbose",        no_argument,       NULL, 'v' },
        { "terminate",      no_argument,       NULL, 't' },
        { "cache",          no_argument,       NULL, 'c' },
        { "ignore-local",   no_argument,       NULL, 'l' },
        { "resolve",        no_argument,       NULL, 'r' },
        { "no-fail",        no_argument,       NULL, 'f' },
        { "parsable",      no_argument,       NULL, 'p' },
        { "color",          optional_argument, NULL, ARG_COLOR },
#if defined(HAVE_GDBM)
        { "no-db-lookup",   no_argument,       NULL, 'k' },
        { "dump-db",        no_argument,       NULL, 'b' },
#endif
        { NULL, 0, NULL, 0 }
    };

    assert(c);

    c->command = strstr(argv0, "domain") ? COMMAND_BROWSE_DOMAINS : COMMAND_BROWSE_SERVICES;
    c->verbose =
        c->terminate_on_cache_exhausted =
        c->terminate_on_all_for_now =
        c->ignore_local =
        c->resolve =
        c->no_fail =
        c->parsable = 0;
    c->color = COLOR_AUTO;
    c->domain = c->stype = NULL;

#if defined(HAVE_GDBM)
    c->no_db_lookup = 0;
#endif

    while ((o = getopt_long(argc, argv, "hVd:avtclrDfp"
#if defined(HAVE_GDBM)
                            "kb"
#endif
                            , long_options, NULL)) >= 0) {

        switch(o) {
            case 'h':
                c->command = COMMAND_HELP;
                break;
            case 'V':
                c->command = COMMAND_VERSION;
                break;
            case 'a':
                c->command = COMMAND_BROWSE_ALL_SERVICES;
                break;
            case 'D':
                c->command = COMMAND_BROWSE_DOMAINS;
                break;
            case 'd':
                avahi_free(c->domain);
                c->domain = avahi_strdup(optarg);
                break;
            case 'v':
                c->verbose = 1;
                break;
            case 't':
                c->terminate_on_all_for_now = 1;
                break;
            case 'c':
                c->terminate_on_cache_exhausted = 1;
                break;
            case 'l':
                c->ignore_local = 1;
                break;
            case 'r':
                c->resolve = 1;
                break;
            case 'f':
                c->no_fail = 1;
                break;
            case 'p':
                c->parsable = 1;
                break;
            case ARG_COLOR:
                if (!optarg || strcmp(optarg, "always") == 0)
                    c->color = COLOR_ALWAYS;
                else if (strcmp(optarg, "never") == 0)
                    c->color = COLOR_NEVER;
                else if (strcmp(optarg, "auto") == 0)
                    c->color = COLOR_AUTO;
                else {
                    eprintf(_("Invalid --color argument '%s', expected auto, always or never\n"), optarg);
                    return -1;
                }
                break;
#if defined(HAVE_GDBM)
            case 'k':
                c->no_db_lookup = 1;
                break;
            case 'b':
                c->command = COMMAND_DUMP_STDB;
                break;
#endif
            default:
                return -1;
        }
    }

    if (c->command == COMMAND_BROWSE_SERVICES) {
        if (optind >= argc) {
            eprintf(_("Too few arguments\n"));
            return -1;
        }

        c->stype = avahi_strdup(argv[optind]);
        optind++;
    }

    if (optind < argc) {
        eprintf(_("Too many arguments\n"));
        return -1;
    }

    return 0;
}

static int want_color(const Config *c) {
    const char *e;

    if (c->parsable || c->color == COLOR_NEVER)
        return 0;

    if (c->color == COLOR_ALWAYS)
        return 1;

    /* https://no-color.org/ */
    if ((e = getenv("NO_COLOR")) && *e)
        return 0;

    if ((e = getenv("TERM")) && strcmp(e, "dumb") == 0)
        return 0;

    return isatty(STDOUT_FILENO);
}

static void setup_terminal(const Config *c) {
    const char *e;

    use_color = want_color(c);
    pretty = !c->parsable && isatty(STDOUT_FILENO);
    use_spinner = pretty && isatty(STDERR_FILENO) && !((e = getenv("TERM")) && strcmp(e, "dumb") == 0);

    update_columns();

    if (pretty && !columns_from_env) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigwinch_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGWINCH, &sa, NULL);
    }
}

int main(int argc, char *argv[]) {
    int ret = 1, error;
    Config config;
    const char *argv0;

    avahi_init_i18n();
    setlocale(LC_ALL, "");
    utf8 = strcmp(nl_langinfo(CODESET), "UTF-8") == 0;

    if ((argv0 = strrchr(argv[0], '/')))
        argv0++;
    else
        argv0 = argv[0];

    if (parse_command_line(&config, argv0, argc, argv) < 0)
        goto fail;

    setup_terminal(&config);

    switch (config.command) {
        case COMMAND_HELP:
            help(stdout, argv0);
            ret = 0;
            break;

        case COMMAND_VERSION:
            printf("%s "PACKAGE_VERSION"\n", argv0);
            ret = 0;
            break;

        case COMMAND_BROWSE_SERVICES:
        case COMMAND_BROWSE_ALL_SERVICES:
        case COMMAND_BROWSE_DOMAINS:

            if (!(simple_poll = avahi_simple_poll_new())) {
                eprintf(_("Failed to create simple poll object.\n"));
                goto fail;
            }

            if (sigint_install(simple_poll) < 0)
                goto fail;

            sigint_set_before_quit(spinner_clear);

            if (!(client = avahi_client_new(avahi_simple_poll_get(simple_poll), config.no_fail ? AVAHI_CLIENT_NO_FAIL : 0, client_callback, &config, &error))) {
                eprintf(_("Failed to create client object: %s\n"), avahi_strerror(error));
                goto fail;
            }

            avahi_simple_poll_loop(simple_poll);
            spinner_stop();
            ret = 0;
            break;

#if defined(HAVE_GDBM)
        case COMMAND_DUMP_STDB: {
            char *t;
            stdb_setent();

            while ((t = stdb_getent())) {
                if (config.no_db_lookup)
                    printf("%s\n", t);
                else
                    printf("%s\n", stdb_lookup(t));
            }

            ret = 0;
            break;
        }
#endif
    }


fail:

    while (services)
        remove_service(&config, services);

    if (client)
        avahi_client_free(client);

    sigint_uninstall();

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    avahi_free(config.domain);
    avahi_free(config.stype);

    avahi_string_list_free(browsed_types);

#if defined(HAVE_GDBM)
    stdb_shutdown();
#endif

    return ret;
}

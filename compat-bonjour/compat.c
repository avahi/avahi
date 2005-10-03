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

#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>
#include <avahi-client/client.h>

#include "warn.h"
#include "dns_sd.h"

enum {
    COMMAND_POLL = 'P',
    COMMAND_QUIT = 'Q',
    COMMAND_POLLED = 'D'
};

struct _DNSServiceRef_t {
    AvahiSimplePoll *simple_poll;

    int thread_fd, main_fd;

    pthread_t thread;
    int thread_running;

    pthread_mutex_t mutex;

    void *context;
    DNSServiceBrowseReply service_browser_callback;
    DNSServiceResolveReply service_resolver_callback;

    AvahiClient *client;
    AvahiServiceBrowser *service_browser;
    AvahiServiceResolver *service_resolver;
};

#define ASSERT_SUCCESS(r) { int __ret = (r); assert(__ret == 0); }

static int read_command(int fd) {
    ssize_t r;
    char command;

    assert(fd >= 0);
    
    if ((r = read(fd, &command, 1)) != 1) {
        fprintf(stderr, __FILE__": read() failed: %s\n", r < 0 ? strerror(errno) : "EOF");
        return -1;
    }

    return command;
}

static int write_command(int fd, char reply) {
    assert(fd >= 0);

    if (write(fd, &reply, 1) != 1) {
        fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int poll_func(struct pollfd *ufds, unsigned int nfds, int timeout, void *userdata) {
    DNSServiceRef sdref = userdata;
    int ret;
    
    assert(sdref);
    
    ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));

/*     fprintf(stderr, "pre-syscall\n"); */
    ret = poll(ufds, nfds, timeout);
/*     fprintf(stderr, "post-syscall\n"); */
    
    ASSERT_SUCCESS(pthread_mutex_lock(&sdref->mutex));

    return ret;
}

static void * thread_func(void *data) {
    DNSServiceRef sdref = data;
    sigset_t mask;

    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    
    sdref->thread = pthread_self();
    sdref->thread_running = 1;

    for (;;) {
        char command;

        if ((command = read_command(sdref->thread_fd)) < 0)
            break;

/*         fprintf(stderr, "Command: %c\n", command); */
        
        switch (command) {

            case COMMAND_POLL:

                ASSERT_SUCCESS(pthread_mutex_lock(&sdref->mutex));
                    
                
                if (avahi_simple_poll_run(sdref->simple_poll) < 0) {
                    fprintf(stderr, __FILE__": avahi_simple_poll_run() failed.\n");
                    ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));
                    break;
                }

                if (write_command(sdref->thread_fd, COMMAND_POLLED) < 0) {
                    ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));
                    break;
                }

                ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));
                
                break;

            case COMMAND_QUIT:
                return NULL;
        }
        
    }

    return NULL;
}

static DNSServiceRef sdref_new(void) {
    int fd[2] = { -1, -1 };
    DNSServiceRef sdref = NULL;
    pthread_mutexattr_t mutex_attr;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0)
        goto fail;

    if (!(sdref = avahi_new(struct _DNSServiceRef_t, 1)))
        goto fail;

    sdref->thread_fd = fd[0];
    sdref->main_fd = fd[1];

    sdref->client = NULL;
    sdref->service_browser = NULL;
    sdref->service_resolver = NULL;

    ASSERT_SUCCESS(pthread_mutexattr_init(&mutex_attr));
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    ASSERT_SUCCESS(pthread_mutex_init(&sdref->mutex, NULL));

    sdref->thread_running = 0;

    if (!(sdref->simple_poll = avahi_simple_poll_new()))
        goto fail;

    avahi_simple_poll_set_func(sdref->simple_poll, poll_func, sdref);

    /* Start simple poll */
    if (avahi_simple_poll_prepare(sdref->simple_poll, -1) < 0)
        goto fail;

    /* Queue a initiall POLL command for the thread */
    if (write_command(sdref->main_fd, COMMAND_POLL) < 0)
        goto fail;
    
    if (pthread_create(&sdref->thread, NULL, thread_func, sdref) != 0)
        goto fail;

    sdref->thread_running = 1;
    
    return sdref;

fail:

    if (sdref)
        DNSServiceRefDeallocate(sdref);

    return NULL;
}

int DNSSD_API DNSServiceRefSockFD(DNSServiceRef sdRef) {
    assert(sdRef);

    AVAHI_WARN_LINKAGE;
    
    return sdRef->main_fd;
}

DNSServiceErrorType DNSSD_API DNSServiceProcessResult(DNSServiceRef sdref) {
    DNSServiceErrorType ret = kDNSServiceErr_Unknown;

    AVAHI_WARN_LINKAGE;

    ASSERT_SUCCESS(pthread_mutex_lock(&sdref->mutex));
    
    /* Cleanup notification socket */
    if (read_command(sdref->main_fd) != COMMAND_POLLED)
        goto finish;
    
    if (avahi_simple_poll_dispatch(sdref->simple_poll) < 0)
        goto finish;
    
    if (avahi_simple_poll_prepare(sdref->simple_poll, -1) < 0)
        goto finish;

    /* Request the poll */
    if (write_command(sdref->main_fd, COMMAND_POLL) < 0)
        goto finish;
    
    ret = kDNSServiceErr_NoError;
    
finish:

    ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));
    
    return ret;
}

void DNSSD_API DNSServiceRefDeallocate(DNSServiceRef sdref) {
    assert(sdref);

    AVAHI_WARN_LINKAGE;

    if (sdref->thread_running) {
        write_command(sdref->main_fd, COMMAND_QUIT);
        avahi_simple_poll_wakeup(sdref->simple_poll);
        pthread_join(sdref->thread, NULL);
    }

    if (sdref->client)
        avahi_client_free(sdref->client);

    if (sdref->thread_fd >= 0)
        close(sdref->thread_fd);

    if (sdref->main_fd >= 0)
        close(sdref->main_fd);

    if (sdref->simple_poll)
        avahi_simple_poll_free(sdref->simple_poll);

    pthread_mutex_destroy(&sdref->mutex);
    
    avahi_free(sdref);
}

static DNSServiceErrorType map_error(int error) {
    switch (error) {
        case AVAHI_OK :
            return kDNSServiceErr_NoError;
            
        case AVAHI_ERR_BAD_STATE :
            return kDNSServiceErr_BadState;
            
        case AVAHI_ERR_INVALID_HOST_NAME:
        case AVAHI_ERR_INVALID_DOMAIN_NAME:
        case AVAHI_ERR_INVALID_TTL:
        case AVAHI_ERR_IS_PATTERN:
        case AVAHI_ERR_INVALID_RECORD:
        case AVAHI_ERR_INVALID_SERVICE_NAME:
        case AVAHI_ERR_INVALID_SERVICE_TYPE:
        case AVAHI_ERR_INVALID_PORT:
        case AVAHI_ERR_INVALID_KEY:
        case AVAHI_ERR_INVALID_ADDRESS:
            return kDNSServiceErr_BadParam;


        case AVAHI_ERR_LOCAL_COLLISION:
            return kDNSServiceErr_NameConflict;

        case AVAHI_ERR_TOO_MANY_CLIENTS:
        case AVAHI_ERR_TOO_MANY_OBJECTS:
        case AVAHI_ERR_TOO_MANY_ENTRIES:
        case AVAHI_ERR_ACCESS_DENIED:
            return kDNSServiceErr_Refused;

        case AVAHI_ERR_INVALID_OPERATION:
        case AVAHI_ERR_INVALID_OBJECT:
            return kDNSServiceErr_Invalid;

        case AVAHI_ERR_NO_MEMORY:
            return kDNSServiceErr_NoMemory;

        case AVAHI_ERR_INVALID_INTERFACE:
        case AVAHI_ERR_INVALID_PROTOCOL:
            return kDNSServiceErr_BadInterfaceIndex;
        
        case AVAHI_ERR_INVALID_FLAGS:
            return kDNSServiceErr_BadFlags;
            
        case AVAHI_ERR_NOT_FOUND:
            return kDNSServiceErr_NoSuchName;
            
        case AVAHI_ERR_VERSION_MISMATCH:
            return kDNSServiceErr_Incompatible;

        case AVAHI_ERR_NO_NETWORK:
        case AVAHI_ERR_OS:
        case AVAHI_ERR_INVALID_CONFIG:
        case AVAHI_ERR_TIMEOUT:
        case AVAHI_ERR_DBUS_ERROR:
        case AVAHI_ERR_NOT_CONNECTED:
        case AVAHI_ERR_NO_DAEMON:
            break;

    }

    return kDNSServiceErr_Unknown;
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

    DNSServiceRef sdref = userdata;

    assert(b);
    assert(sdref);

    switch (event) {
        case AVAHI_BROWSER_NEW:
            sdref->service_browser_callback(sdref, kDNSServiceFlagsAdd, interface, kDNSServiceErr_NoError, name, type, domain, sdref->context);
            break;

        case AVAHI_BROWSER_REMOVE:
            sdref->service_browser_callback(sdref, 0, interface, kDNSServiceErr_NoError, name, type, domain, sdref->context);
            break;

        case AVAHI_BROWSER_FAILURE:
            sdref->service_browser_callback(sdref, 0, interface, kDNSServiceErr_Unknown, name, type, domain, sdref->context);
            break;
            
        case AVAHI_BROWSER_NOT_FOUND:
            sdref->service_browser_callback(sdref, 0, interface, kDNSServiceErr_NoSuchName, name, type, domain, sdref->context);
            break;
            
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
        case AVAHI_BROWSER_ALL_FOR_NOW:
            break;
    }
}

DNSServiceErrorType DNSSD_API DNSServiceBrowse(
    DNSServiceRef *ret_sdref,
    DNSServiceFlags flags,
    uint32_t interface,
    const char *regtype,
    const char *domain,
    DNSServiceBrowseReply callback,
    void *context) {

    DNSServiceErrorType ret = kDNSServiceErr_Unknown;
    int error;
    DNSServiceRef sdref = NULL;
    AvahiIfIndex ifindex;
    
    AVAHI_WARN_LINKAGE;
    
    assert(ret_sdref);
    assert(regtype);
    assert(domain);
    assert(callback);

    if (interface == kDNSServiceInterfaceIndexLocalOnly || flags != 0)
        return kDNSServiceErr_Unsupported;

    if (!(sdref = sdref_new()))
        return kDNSServiceErr_Unknown;

    sdref->context = context;
    sdref->service_browser_callback = callback;

    ASSERT_SUCCESS(pthread_mutex_lock(&sdref->mutex));
    
    if (!(sdref->client = avahi_client_new(avahi_simple_poll_get(sdref->simple_poll), NULL, NULL, &error))) {
        ret =  map_error(error);
        goto finish;
    }

    ifindex = interface == kDNSServiceInterfaceIndexAny ? AVAHI_IF_UNSPEC : (AvahiIfIndex) interface;
    
    if (!(sdref->service_browser = avahi_service_browser_new(sdref->client, ifindex, AVAHI_PROTO_UNSPEC, regtype, domain, 0, service_browser_callback, sdref))) {
        ret = map_error(avahi_client_errno(sdref->client));
        goto finish;
    }
    

    ret = kDNSServiceErr_NoError;
    *ret_sdref = sdref;
                                                              
finish:

    ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));
    
    if (ret != kDNSServiceErr_NoError)
        DNSServiceRefDeallocate(sdref);

    return ret;
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
    AvahiLookupResultFlags flags,
    void *userdata) {

    DNSServiceRef sdref = userdata;

    assert(r);
    assert(sdref);

    switch (event) {
        case AVAHI_RESOLVER_FOUND: {

            char full_name[kDNSServiceMaxDomainName];
            int ret;
            char *p = NULL;
            size_t l = 0;

            if ((p = avahi_new0(char, (l = avahi_string_list_serialize(txt, NULL, 0))+1)))
                avahi_string_list_serialize(txt, p, l);

            ret = avahi_service_name_snprint(full_name, sizeof(full_name), name, type, domain);
            assert(ret == AVAHI_OK);
            
            sdref->service_resolver_callback(sdref, 0, interface, kDNSServiceErr_NoError, full_name, host_name, htons(port), l, p, sdref->context);

            avahi_free(p);
            break;
        }

        case AVAHI_RESOLVER_TIMEOUT:
        case AVAHI_RESOLVER_NOT_FOUND:
            sdref->service_resolver_callback(sdref, 0, interface, kDNSServiceErr_NoSuchName, NULL, NULL, 0, 0, NULL, sdref->context);
            break;
            
        case AVAHI_RESOLVER_FAILURE:
            sdref->service_resolver_callback(sdref, 0, interface, kDNSServiceErr_Unknown, NULL, NULL, 0, 0, NULL, sdref->context);
            
    }
}

DNSServiceErrorType DNSSD_API DNSServiceResolve(
    DNSServiceRef *ret_sdref,
    DNSServiceFlags flags,
    uint32_t interface,
    const char *name,
    const char *regtype,
    const char *domain,
    DNSServiceResolveReply callback,
    void *context) {

    DNSServiceErrorType ret = kDNSServiceErr_Unknown;
    int error;
    DNSServiceRef sdref = NULL;
    AvahiIfIndex ifindex;

    AVAHI_WARN_LINKAGE;

    assert(ret_sdref);
    assert(name);
    assert(regtype);
    assert(domain);
    assert(callback);

    if (interface == kDNSServiceInterfaceIndexLocalOnly || flags != 0)
        return kDNSServiceErr_Unsupported;

    if (!(sdref = sdref_new()))
        return kDNSServiceErr_Unknown;

    sdref->context = context;
    sdref->service_resolver_callback = callback;

    ASSERT_SUCCESS(pthread_mutex_lock(&sdref->mutex));
    
    if (!(sdref->client = avahi_client_new(avahi_simple_poll_get(sdref->simple_poll), NULL, NULL, &error))) {
        ret =  map_error(error);
        goto finish;
    }

    ifindex = interface == kDNSServiceInterfaceIndexAny ? AVAHI_IF_UNSPEC : (AvahiIfIndex) interface;
    
    if (!(sdref->service_resolver = avahi_service_resolver_new(sdref->client, ifindex, AVAHI_PROTO_UNSPEC, name, regtype, domain, AVAHI_PROTO_UNSPEC, 0, service_resolver_callback, sdref))) {
        ret = map_error(avahi_client_errno(sdref->client));
        goto finish;
    }
    

    ret = kDNSServiceErr_NoError;
    *ret_sdref = sdref;
                                                              
finish:

    ASSERT_SUCCESS(pthread_mutex_unlock(&sdref->mutex));
    
    if (ret != kDNSServiceErr_NoError)
        DNSServiceRefDeallocate(sdref);

    return ret;
}

int DNSSD_API DNSServiceConstructFullName (
    char *fullName,
    const char *service,   
    const char *regtype,
    const char *domain) {

    AVAHI_WARN_LINKAGE;

    assert(fullName);
    assert(regtype);
    assert(domain);

    if (avahi_service_name_snprint(fullName, kDNSServiceMaxDomainName, service, regtype, domain) < 0)
        return -1;
    
    return 0;
}


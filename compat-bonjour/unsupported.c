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

#include "dns_sd.h"

DNSServiceErrorType DNSSD_API DNSServiceEnumerateDomains(
    DNSServiceRef *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceDomainEnumReply callBack,
    void *context) {

    return kDNSServiceErr_Unsupported;
}

DNSServiceErrorType DNSSD_API DNSServiceRegister (
    DNSServiceRef *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char *name,        
    const char *regtype,
    const char *domain,      
    const char *host,        
    uint16_t port,
    uint16_t txtLen,
    const void *txtRecord,   
    DNSServiceRegisterReply callBack,    
    void *context) {

    return kDNSServiceErr_Unsupported;
}

int DNSSD_API DNSServiceConstructFullName (
    char *fullName,
    const char *service,   
    const char *regtype,
    const char *domain) {

    return kDNSServiceErr_Unsupported;
}

DNSServiceErrorType DNSSD_API DNSServiceRegisterRecord (
    DNSServiceRef sdRef,
    DNSRecordRef *RecordRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    uint16_t rdlen,
    const void *rdata,
    uint32_t ttl,
    DNSServiceRegisterRecordReply callBack,
    void *context) {

    return kDNSServiceErr_Unsupported;
}

DNSServiceErrorType DNSSD_API DNSServiceQueryRecord (
    DNSServiceRef *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    DNSServiceQueryRecordReply callBack,
    void *context) {

    return kDNSServiceErr_Unsupported;
}

void DNSSD_API DNSServiceReconfirmRecord (
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    uint16_t rdlen,
    const void *rdata) {

    return;
}

DNSServiceErrorType DNSSD_API DNSServiceCreateConnection(DNSServiceRef *sdRef) {
    return kDNSServiceErr_Unsupported;
}

DNSServiceErrorType DNSSD_API DNSServiceAddRecord(
    DNSServiceRef sdRef,
    DNSRecordRef *RecordRef,
    DNSServiceFlags flags,
    uint16_t rrtype,
    uint16_t rdlen,
    const void *rdata,
    uint32_t ttl) {

    return kDNSServiceErr_Unsupported;
}

DNSServiceErrorType DNSSD_API DNSServiceUpdateRecord(
    DNSServiceRef sdRef,
    DNSRecordRef RecordRef,     
    DNSServiceFlags flags,
    uint16_t rdlen,
    const void *rdata,
    uint32_t ttl) {

    return kDNSServiceErr_Unsupported;
}

DNSServiceErrorType DNSSD_API DNSServiceRemoveRecord(
    DNSServiceRef sdRef,
    DNSRecordRef RecordRef,
    DNSServiceFlags flags) {

    return kDNSServiceErr_Unsupported;
}



/*
 * ps5-native-app-boilerplate - libcurl support for PS5 native titles.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What PacBrew's libcurl 8.18.0 / OpenSSL 3.5.2 archives need to run in a
 * native title; see console_curl.h. Taken from ProsperoRadio (elevated) and
 * ProsperoLichess (sandboxed), where each piece was found on hardware. When a
 * PacBrew update brings new undefined symbols, the linker names them; a symbol
 * that links but resolves to libScePosixForWebKit is a null import at run time
 * and needs a definition here too.
 */

#include "console_curl.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

extern int sceNetPoolCreate(const char *name, int size, int flags);
extern int sceNetPoolDestroy(int pool);
extern int sceNetResolverCreate(const char *name, int pool, int flags);
extern int sceNetResolverStartNtoa(int resolver, const char *hostname, struct in_addr *address,
                                   int timeout_us, int retries, int flags);
extern int sceNetResolverDestroy(int resolver);

/* Bundled Mozilla roots; never skip peer or host-name verification. */
const char *console_curl_ca_file(void)
{
    return "/app0/romfs/cacert.pem";
}

/* ---- Per-handle setup ---------------------------------------------------------------------- */

enum
{
    CONSOLE_SO_NBIO = 0x1200 /* the console's own non-blocking socket option */
};

int console_curl_nonblocking(int socket)
{
    const int on = 1;
    return setsockopt(socket, SOL_SOCKET, CONSOLE_SO_NBIO, &on, sizeof(on));
}

static int console_curl_on_socket(void *user, curl_socket_t socket, curlsocktype purpose)
{
    (void)user;
    (void)purpose;
    /* Keep going if it fails: the transfer still works, only slowly. */
    (void)console_curl_nonblocking(socket);
    return CURL_SOCKOPT_OK;
}

void console_curl_setup(CURL *easy)
{
    (void)curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    (void)curl_easy_setopt(easy, CURLOPT_CAINFO, console_curl_ca_file());
    (void)curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, console_curl_on_socket);
}

/* ---- fcntl on sockets ---------------------------------------------------------------------- */

/* In a sandboxed title the console's libc refuses fcntl on sockets with EINVAL.
 * curl then fails every connect ("fcntl set CLOEXEC: Invalid argument") and its
 * sockets stay blocking. Linked with --wrap=fcntl, every fcntl call in the app
 * and the archives comes here; only a refused call is treated as a socket. */

extern int __real_fcntl(int descriptor, int command, ...);

static int console_curl_socket_flags(int socket, int command, int value)
{
    switch (command)
    {
    case F_GETFD:
    case F_SETFD:
        return 0; /* a title never execs another program */
    case F_SETFL:
    {
        const int on = (value & O_NONBLOCK) != 0;
        return setsockopt(socket, SOL_SOCKET, CONSOLE_SO_NBIO, &on, sizeof(on));
    }
    case F_GETFL:
    {
        int on = 0;
        socklen_t length = sizeof(on);
        if (getsockopt(socket, SOL_SOCKET, CONSOLE_SO_NBIO, &on, &length) < 0)
            return -1;
        return O_RDWR | (on ? O_NONBLOCK : 0);
    }
    default:
        errno = EINVAL;
        return -1;
    }
}

int __wrap_fcntl(int descriptor, int command, ...)
{
    va_list arguments;
    va_start(arguments, command);
    /* Every command curl and its libraries use takes an int or a pointer. */
    const intptr_t argument = va_arg(arguments, intptr_t);
    va_end(arguments);
    const int result = __real_fcntl(descriptor, command, argument);
    if (result >= 0 || errno != EINVAL)
        return result;
    if (command == F_GETFD || command == F_SETFD || command == F_SETFL || command == F_GETFL)
        return console_curl_socket_flags(descriptor, command, (int)argument);
    return result;
}

/* ---- Name lookup --------------------------------------------------------------------------- */

/* IPv4, one address per name, through sceNetResolver on the calling thread. */
static int console_curl_lookup(const char *name, struct in_addr *address)
{
    if (inet_pton(AF_INET, name, address) == 1)
        return 0;
    const int pool = sceNetPoolCreate("console_curl_dns", 16 * 1024, 0);
    if (pool < 0)
        return EAI_MEMORY;
    int result = EAI_FAIL;
    const int resolver = sceNetResolverCreate("console_curl_dns", pool, 0);
    if (resolver >= 0)
    {
        /* 5 s per try, 2 retries. */
        result =
            sceNetResolverStartNtoa(resolver, name, address, 5000000, 2, 0) < 0 ? EAI_NONAME : 0;
        (void)sceNetResolverDestroy(resolver);
    }
    (void)sceNetPoolDestroy(pool);
    return result;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
                struct addrinfo **result)
{
    *result = NULL;
    const int family = hints != NULL ? hints->ai_family : AF_UNSPEC;
    if (family != AF_UNSPEC && family != AF_INET)
        return EAI_FAMILY;

    struct in_addr address;
    address.s_addr = htonl(INADDR_LOOPBACK);
    if (node != NULL)
    {
        if (hints != NULL && (hints->ai_flags & AI_NUMERICHOST) != 0)
        {
            if (inet_pton(AF_INET, node, &address) != 1)
                return EAI_NONAME;
        }
        else
        {
            const int failed = console_curl_lookup(node, &address);
            if (failed != 0)
                return failed;
        }
    }
    else if (hints != NULL && (hints->ai_flags & AI_PASSIVE) != 0)
    {
        address.s_addr = htonl(INADDR_ANY);
    }

    /* The entry and its address in one allocation: freeaddrinfo frees once. */
    struct addrinfo *entry = calloc(1, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    if (entry == NULL)
        return EAI_MEMORY;
    struct sockaddr_in *in = (struct sockaddr_in *)(entry + 1);
    in->sin_len = sizeof(*in);
    in->sin_family = AF_INET;
    in->sin_port = htons((uint16_t)(service != NULL ? atoi(service) : 0));
    in->sin_addr = address;
    entry->ai_family = AF_INET;
    entry->ai_socktype =
        hints != NULL && hints->ai_socktype != 0 ? hints->ai_socktype : SOCK_STREAM;
    entry->ai_protocol = hints != NULL ? hints->ai_protocol : 0;
    entry->ai_addrlen = sizeof(*in);
    entry->ai_addr = (struct sockaddr *)in;
    *result = entry;
    return 0;
}

void freeaddrinfo(struct addrinfo *entry)
{
    while (entry != NULL)
    {
        struct addrinfo *next = entry->ai_next;
        free(entry);
        entry = next;
    }
}

const char *gai_strerror(int code)
{
    switch (code)
    {
    case 0:
        return "no error";
    case EAI_NONAME:
        return "the name was not found";
    case EAI_FAMILY:
        return "address family not supported";
    case EAI_MEMORY:
        return "out of memory";
    default:
        return "the name lookup failed";
    }
}

struct hostent *gethostbyname(const char *name)
{
    (void)name;
    return NULL; /* libcurl uses getaddrinfo */
}

/* Numeric only: enough for libcurl's record of the connected address. */
int getnameinfo(const struct sockaddr *address, socklen_t length, char *host, size_t host_size,
                char *service, size_t service_size, int flags)
{
    (void)flags;
    if (address == NULL || address->sa_family != AF_INET || length < sizeof(struct sockaddr_in))
        return EAI_FAMILY;
    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
    if (host != NULL && host_size != 0 &&
        inet_ntop(AF_INET, &in->sin_addr, host, (socklen_t)host_size) == NULL)
        return EAI_FAIL;
    if (service != NULL && service_size != 0)
        snprintf(service, service_size, "%u", (unsigned)ntohs(in->sin_port));
    return 0;
}

/* '*' and '?' only: all libcurl's wildcard matching asks for. */
int fnmatch(const char *pattern, const char *text, int flags)
{
    for (; *pattern != '\0'; ++pattern, ++text)
    {
        if (*pattern == '*')
        {
            while (*pattern == '*')
                ++pattern;
            if (*pattern == '\0')
                return 0;
            for (; *text != '\0'; ++text)
            {
                if (fnmatch(pattern, text, flags) == 0)
                    return 0;
            }
            return FNM_NOMATCH;
        }
        if (*text == '\0' || (*pattern != '?' && *pattern != *text))
            return FNM_NOMATCH;
    }
    return *text == '\0' ? 0 : FNM_NOMATCH;
}


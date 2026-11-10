/*
 * Small POSIX compatibility layer for the prebuilt PacBrew FFmpeg/text ports.
 * The public PS5 SDK declares these APIs but does not export implementations.
 * These are limited C-locale and app-local fallbacks, not a complete libc.
 * GPL-3.0-or-later
 */
#define _DEFAULT_SOURCE 1
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <net/if.h>
#include <poll.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <xlocale/_locale.h>

static char c_locale_token;

locale_t newlocale(int mask, const char *name, locale_t base) {
    (void)mask; (void)base;
    if (name && (!strcmp(name, "C") || !strcmp(name, "POSIX") ||
                 !strcmp(name, "C.UTF-8")))
        return (locale_t)&c_locale_token;
    errno = ENOENT;
    return (locale_t)0;
}

locale_t uselocale(locale_t next) {
    (void)next;
    /* Only the process-wide C locale is available in this SDK build. */
    return LC_GLOBAL_LOCALE;
}

int freelocale(locale_t handle) {
    (void)handle;
    return 0;
}

char *nl_langinfo(nl_item item) {
    static char charset[] = "UTF-8";
    static char date_fmt[] = "%m/%d/%y";
    static char time_fmt[] = "%H:%M:%S";
    static char empty[] = "";
    switch (item) {
        case CODESET: return charset;
        case D_FMT: return date_fmt;
        case T_FMT: return time_fmt;
        default: return empty;
    }
}

int ___mb_cur_max(void) {
    return __mb_cur_max > 0 ? __mb_cur_max : 1;
}

struct tm *gmtime_r(const time_t *value, struct tm *result) {
    if (!value || !result) { errno = EINVAL; return NULL; }
    struct tm *source = gmtime(value);
    if (!source) return NULL;
    *result = *source;
    return result;
}

struct tm *localtime_r(const time_t *value, struct tm *result) {
    if (!value || !result) { errno = EINVAL; return NULL; }
    struct tm *source = localtime(value);
    if (!source) return NULL;
    *result = *source;
    return result;
}

int mkostemp(char *name, int flags) {
    int fd = mkstemp(name);
    if (fd < 0) return fd;
    if ((flags & O_CLOEXEC) && fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) goto fail;
    if (flags & (O_APPEND | O_NONBLOCK)) {
        int old = fcntl(fd, F_GETFL);
        if (old < 0 ||
            fcntl(fd, F_SETFL, old | (flags & (O_APPEND | O_NONBLOCK))) == -1)
            goto fail;
    }
    return fd;
fail:
    {
        int saved = errno;
        close(fd);
        unlink(name);
        errno = saved;
        return -1;
    }
}

int pipe2(int out[2], int flags) {
    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) { errno = EINVAL; return -1; }
    if (pipe(out) < 0) return -1;
    if (((flags & O_CLOEXEC) &&
         (fcntl(out[0], F_SETFD, FD_CLOEXEC) < 0 ||
          fcntl(out[1], F_SETFD, FD_CLOEXEC) < 0)) ||
        ((flags & O_NONBLOCK) &&
         (fcntl(out[0], F_SETFL, O_NONBLOCK) < 0 ||
          fcntl(out[1], F_SETFL, O_NONBLOCK) < 0))) {
        int saved = errno;
        close(out[0]); close(out[1]);
        errno = saved;
        return -1;
    }
    return 0;
}

int dladdr(const void *address, Dl_info *info) {
    (void)address;
    if (info) memset(info, 0, sizeof(*info));
    return 0;  /* Native loader symbol introspection is unavailable. */
}

unsigned int if_nametoindex(const char *name) {
    (void)name;
    errno = ENXIO;  /* Interface-name lookup is not exposed by this SDK. */
    return 0;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer,
               size_t size, struct passwd **result) {
    (void)uid; (void)pwd; (void)buffer; (void)size;
    if (result) *result = NULL;
    return ENOENT;  /* PS5 homebrew has no host passwd database. */
}

ssize_t sendmmsg(int fd, struct mmsghdr *messages, size_t count, int flags) {
    size_t sent = 0;
    if (!messages || !count) { errno = EINVAL; return -1; }
    for (; sent < count; ++sent) {
        ssize_t bytes = sendmsg(fd, &messages[sent].msg_hdr, flags);
        if (bytes < 0) return sent ? (ssize_t)sent : -1;
        messages[sent].msg_len = bytes;
    }
    return (ssize_t)sent;
}

ssize_t recvmmsg(int fd, struct mmsghdr *messages, size_t count,
                 int flags, const struct timespec *timeout) {
    if (!messages || !count) { errno = EINVAL; return -1; }
    if (timeout) {
        int wait_ms = (int)(timeout->tv_sec > 2147483 ? 2147483647 :
                     timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000);
        struct pollfd input = {fd, POLLIN, 0};
        int waited = poll(&input, 1, wait_ms);
        if (waited < 0) return -1;
        if (!waited) { errno = EAGAIN; return -1; }
    }
    size_t received = 0;
    for (; received < count; ++received) {
        /* After one packet, do not hang while filling the whole batch. */
        int recv_flags = flags & ~MSG_WAITFORONE;
        if (received) recv_flags |= MSG_DONTWAIT;
        ssize_t bytes = recvmsg(fd, &messages[received].msg_hdr, recv_flags);
        if (bytes < 0) return received ? (ssize_t)received : -1;
        messages[received].msg_len = bytes;
    }
    return (ssize_t)received;
}

/* Tail-jumps preserve the original jmp_buf caller frame and SysV ABI.
 * Implementing these as C wrappers around setjmp would be undefined behavior.
 * PS5 native title code is x86_64, just like the public payload SDK. */
#if defined(__x86_64__)
__asm__(".globl _setjmp\n.type _setjmp,@function\n_setjmp:\n jmp setjmp\n"
        ".globl _longjmp\n.type _longjmp,@function\n_longjmp:\n jmp longjmp\n");
#endif

/* libzstd's optional weak tracing callbacks are unresolved in the PacBrew
 * archive. A zero context explicitly disables tracing; decoding is unchanged.
 * Keep the public zstd_trace.h calling convention (64-bit context). */
unsigned long long ZSTD_trace_compress_begin(const void *context) {
    (void)context;
    return 0;
}

unsigned long long ZSTD_trace_decompress_begin(const void *context) {
    (void)context;
    return 0;
}

void ZSTD_trace_compress_end(unsigned long long context, const void *trace) {
    (void)context;
    (void)trace;
}

void ZSTD_trace_decompress_end(unsigned long long context, const void *trace) {
    (void)context;
    (void)trace;
}

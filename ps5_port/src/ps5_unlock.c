/*
 * Silent Hill - asking the HEN to let Silent Hill list /data.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A sandboxed title can open and write files under /data but can't list
 * folders. Several services lift that, depending on what the console runs:
 *  - LegacyJB (Phoenixx) and OnionHEN watch each sandbox for a request file,
 *    {"PID":<pid>} in /download0/etahen_jailbreak, for any app;
 *  - PS5SX2 Helper answers the same file, but only for the title IDs in
 *    /data/whitelist.txt (Settings > System can add Silent Hill there);
 *  - etaHEN answers a HijackerCommand on TCP 127.0.0.1:9028 when its "Legacy
 *    CMD server" setting is on (LegacyJB listens there too).
 * The file goes first; the TCP command when nothing answered it. Porpoise was killed right after its grant; it asks from a process that
 * already runs threads, so Silent Hill asks first thing in main(), single-threaded,
 * after giving itself its own credential.
 *
 * A marker file guards against a crash loop: it is written before asking and
 * removed afterwards. If Silent Hill finds it at start-up, the previous request
 * killed the app, so this route is skipped and Silent Hill runs sandboxed.
 */
#if defined(__PROSPERO__)
#include "ps5_unlock.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>

int sceKernelUsleep(unsigned microseconds);
int sceKernelOpen(const char *path, int flags, unsigned short mode);
int sceKernelClose(int fd);

static const char *const REQUESTS[] = {
    "/download0/etahen_jailbreak",   /* etaHEN, OnionHEN, old Lapy */
    "/download0/onionhen_jailbreak", /* OnionHEN also takes this name */
};
#define REQUEST_COUNT (int)(sizeof(REQUESTS) / sizeof(REQUESTS[0]))
#define CRASH_MARKER "/download0/SilentHill_unlock_attempt"

/* /download0 belongs to the title's download data, which the system mounts shortly after the
 * process starts; the request file can only be written once it is there. */
static bool wait_download0(int max_ms)
{
    struct stat st;
    int         waited;

    for (waited = 0; waited <= max_ms; waited += 50)
    {
        if (stat("/download0", &st) == 0)
        {
            ps5_unlock_trace_add("/download0 present after %d ms (mode %o uid %d, euid %d); ", waited,
                                 (unsigned)st.st_mode, (int)st.st_uid, (int)geteuid());
            return true;
        }
        sceKernelUsleep(50000);
    }
    ps5_unlock_trace_add("/download0 never appeared (%d ms, errno=%d); ", max_ms, errno);
    return false;
}

bool ps5_data_listable(void)
{
    /* Opening the folder is not enough: the sandbox lets it open and refuses to read it. */
    DIR *d = opendir("/data");
    if (!d)
        return false;
    struct dirent *e = readdir(d);
    closedir(d);
    return e != NULL;
}

/* What the unlock tried and got, for the log: it runs before the log file exists. */
static char s_trace[1200];

void ps5_unlock_trace_add(const char *fmt, ...)
{
    size_t used = strlen(s_trace);
    va_list ap;

    if (used + 8 >= sizeof(s_trace))
        return;
    va_start(ap, fmt);
    vsnprintf(s_trace + used, sizeof(s_trace) - used, fmt, ap);
    va_end(ap);
}

const char *ps5_unlock_trace(void)
{
    return s_trace;
}

static bool publish(const char *path, int pid)
{
    char staged[64];
    snprintf(staged, sizeof(staged), "%s.tmp", path);
    unlink(staged);
    unlink(path);
    int fd = open(staged, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        int err = errno;
        int kfd = sceKernelOpen(staged, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        ps5_unlock_trace_add("publish(%s) open failed errno=%d, sceKernelOpen=0x%x; ", path, err, (unsigned)kfd);
        if (kfd >= 0)
            sceKernelClose(kfd);
        return false;
    }
    fchmod(fd, 0666);
    char body[32];
    int n = snprintf(body, sizeof(body), "{\"PID\":%d}\n", pid);
    bool ok = write(fd, body, (size_t)n) == n && fsync(fd) == 0;
    close(fd);
    if (!ok || rename(staged, path) != 0)
    {
        ps5_unlock_trace_add("publish(%s) write/rename failed errno=%d; ", path, errno);
        unlink(staged);
        return false;
    }
    ps5_unlock_trace_add("publish(%s) ok pid=%d; ", path, pid);
    return true;
}

/* etaHEN's legacy command server (README: "Jailbreaking an app ... non-
 * whitelist method"). Same layout as its HijackerCommand. */
struct HijackerCommand
{
    int magic; /* 0xDEADBEEF */
    int cmd;   /* JAILBREAK_CMD = 5 */
    int pid;
    int ret;   /* -1337 until answered */
    char msg1[0x500];
    char msg2[0x500];
};

static bool request_over_tcp(int pid)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return false;
    struct timeval tv = {2, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_len = sizeof(a);
    a.sin_family = AF_INET;
    a.sin_port = htons(9028);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bool ok = false;
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0)
    {
        static struct HijackerCommand c;
        memset(&c, 0, sizeof(c));
        c.magic = (int)0xDEADBEEF;
        c.cmd = 5;
        c.pid = pid;
        c.ret = -1337;
        if (send(s, &c, sizeof(c), 0) == (ssize_t)sizeof(c))
        {
            size_t got = 0;
            while (got < sizeof(c))
            {
                ssize_t n = recv(s, (char *)&c + got, sizeof(c) - got, 0);
                if (n <= 0)
                    break;
                got += (size_t)n;
            }
            ok = got >= 16 && (c.ret == 0 || c.ret == -1337);
        }
    }
    close(s);
    return ok;
}

static bool wait_listable(int polls)
{
    for (int poll = 0; poll < polls; ++poll)
    {
        sceKernelUsleep(16667);
        if (ps5_data_listable())
            return true;
    }
    return false;
}

UnlockResult ps5_unlock_etahen(void)
{
    if (ps5_data_listable())
        return UNLOCK_ALREADY;
    if (access(CRASH_MARKER, F_OK) == 0)
    {
        /* The previous launch died while asking: skip this one launch (no crash
         * loop), then ask again on the next. */
        unlink(CRASH_MARKER);
        return UNLOCK_SKIPPED_AFTER_CRASH;
    }
    int marker = open(CRASH_MARKER, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (marker >= 0)
    {
        fsync(marker);
        close(marker);
    }

    seteuid(geteuid()); /* own credential before the HEN changes it */
    UnlockResult result = UNLOCK_NO_ANSWER;
    const int pid = (int)getpid();
    for (int round = 0; round < 3 && result != UNLOCK_OK; ++round)
    {
        int published = 0;
        for (int i = 0; i < REQUEST_COUNT; ++i)
            published += publish(REQUESTS[i], pid);
        if (published == 0)
        {
            /* The sandbox may refuse to create the request file (EACCES under /download0);
             * LegacyJB and etaHEN also answer the same request over TCP port 9028. */
            const bool sent = request_over_tcp(pid);

            ps5_unlock_trace_add("tcp request=%d; ", (int)sent);
            result = (sent && wait_listable(90)) ? UNLOCK_OK : UNLOCK_CANT_REQUEST;
            break;
        }
        /* ~1.5 s for a service to act and /data to open up */
        if (wait_listable(90))
            result = UNLOCK_OK;
        for (int i = 0; i < REQUEST_COUNT; ++i)
            unlink(REQUESTS[i]);
        if (result != UNLOCK_OK && round == 0 && request_over_tcp(pid) && wait_listable(60))
            result = UNLOCK_OK; /* etaHEN's command server */
    }
    unlink(CRASH_MARKER); /* we survived the request either way */
    return result;
}

UnlockResult ps5_unlock_quick(void)
{
    if (ps5_data_listable())
        return UNLOCK_ALREADY;
    wait_download0(4000);
    if (access(CRASH_MARKER, F_OK) == 0)
        return UNLOCK_SKIPPED_AFTER_CRASH; /* ps5_unlock_etahen clears it and skips once */
    int marker = open(CRASH_MARKER, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (marker >= 0)
    {
        fsync(marker);
        close(marker);
    }
    seteuid(geteuid()); /* own credential before the HEN changes it */
    const int pid = (int)getpid();
    int published = 0;
    for (int i = 0; i < REQUEST_COUNT; ++i)
        published += publish(REQUESTS[i], pid);
    UnlockResult result = UNLOCK_CANT_REQUEST;
    if (!published)
    {
        const bool sent = request_over_tcp(pid);

        ps5_unlock_trace_add("tcp request=%d; ", (int)sent);
        if (sent && wait_listable(60))
            result = UNLOCK_OK;
    }
    else
    {
        /* ~0.3 s: a service that is there answers within a few frames */
        result = wait_listable(18) ? UNLOCK_OK : UNLOCK_NO_ANSWER;
        for (int i = 0; i < REQUEST_COUNT; ++i)
            unlink(REQUESTS[i]);
    }
    unlink(CRASH_MARKER);
    return result;
}

void ps5_unlock_reset_crash_marker(void)
{
    unlink(CRASH_MARKER);
}

const char *ps5_unlock_describe(UnlockResult r)
{
    switch (r)
    {
    case UNLOCK_OK: return "granted";
    case UNLOCK_ALREADY: return "already unlocked";
    case UNLOCK_NO_ANSWER: return "no unlock service answered (LegacyJB, etaHEN, PS5SX2 Helper)";
    case UNLOCK_CANT_REQUEST: return "couldn't write the request";
    case UNLOCK_SKIPPED_AFTER_CRASH: return "skipped once after the last request closed Silent Hill (start again to retry)";
    }
    return "?";
}
#endif

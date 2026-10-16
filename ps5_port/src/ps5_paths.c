#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include "sh_log.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char* g_Ps5Root = "/data/SilentHill/";
/* The shipped assets (pc_port/assets) travel inside the title folder. */
static const char* const k_Ps5AssetRoot = "/app0/assets/";

FILE* __real_fopen(const char* path, const char* mode);
int __real_open(const char* path, int flags, ...);
int __real_stat(const char* path, struct stat* st);
int __real_lstat(const char* path, struct stat* st);
int __real_access(const char* path, int mode);
DIR* __real_opendir(const char* path);
struct dirent* __real_readdir(DIR* dir);
int __real_closedir(DIR* dir);
int __real_mkdir(const char* path, mode_t mode);
int __real_remove(const char* path);
int __real_unlink(const char* path);
int __real_rmdir(const char* path);
int __real_rename(const char* from, const char* to);

static const char* fix(const char* path, char* buf, size_t size)
{
    if (!path || path[0] == '/')
        return path;
    while (path[0] == '.' && path[1] == '/')
        path += 2;
    snprintf(buf, size, "%s%s", g_Ps5Root, path);
    return buf;
}

/* Read-only lookups: a file the user has not put under /data/SilentHill falls back to the
 * copy shipped in the title folder. Anything under /data/SilentHill wins, so languages,
 * fonts and the like stay overridable by dropping a file there. */
static const char* fix_ro(const char* path, char* buf, size_t size)
{
    struct stat st;
    char        alt[1024];
    const char* rel = path;

    if (!path || path[0] == '/')
        return path;
    path = fix(path, buf, size);
    if (__real_stat(path, &st) == 0)
        return path;
    while (rel[0] == '.' && rel[1] == '/')
        rel += 2;
    snprintf(alt, sizeof(alt), "%s%s", k_Ps5AssetRoot, rel);
    if (__real_stat(alt, &st) == 0)
    {
        snprintf(buf, size, "%s", alt);
        {
            static int s_logged = 0;
            if (s_logged < 12)
            {
                s_logged++;
                SH_DBG("[PATHS] fallback %s -> %s", rel, alt);
            }
        }
    }
    return buf;
}

#define FIX(p) char p##_buf[1024]; p = fix(p, p##_buf, sizeof(p##_buf))
#define FIX_RO(p) char p##_buf[1024]; p = fix_ro(p, p##_buf, sizeof(p##_buf))

FILE* __wrap_fopen(const char* path, const char* mode)
{
    if (mode && mode[0] == 'r' && !strchr(mode, '+'))
    {
        FIX_RO(path);
        return __real_fopen(path, mode);
    }
    FIX(path);
    return __real_fopen(path, mode);
}

int __wrap_open(const char* path, int flags, ...)
{
    mode_t mode = 0;

    if ((flags & O_ACCMODE) == O_RDONLY && !(flags & O_CREAT))
    {
        FIX_RO(path);
        return __real_open(path, flags, mode);
    }
    FIX(path);
    if (flags & O_CREAT)
    {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    return __real_open(path, flags, mode);
}

int __wrap_stat(const char* path, struct stat* st)
{
    FIX_RO(path);
    return __real_stat(path, st);
}

int __wrap_lstat(const char* path, struct stat* st)
{
    FIX_RO(path);
    return __real_lstat(path, st);
}

int __wrap_access(const char* path, int mode)
{
    if (mode & W_OK)
    {
        FIX(path);
        return __real_access(path, mode);
    }
    FIX_RO(path);
    return __real_access(path, mode);
}

/* The libc opendir goes through the raw getdirentries system call, which the sandbox refuses
 * (EPERM) even for ordinary folders under /data. The kernel library's own calls are allowed,
 * so directories are listed through those. */
int sceKernelOpen(const char* path, int flags, unsigned short mode);
int sceKernelClose(int fd);
int sceKernelGetdents(int fd, char* buf, int nbytes);

typedef struct Ps5Dir
{
    DIR* real; /* set when the libc opendir worked (an unlocked process) */
    int fd;
    int len;
    int pos;
    int size_idx;
    char buf[8192] __attribute__((aligned(16)));
} Ps5Dir;

/* The kernel refuses a read buffer size it dislikes with EINVAL, so smaller ones are tried. */
static const int k_Ps5DentSizes[] = { 8192, 4096, 2048, 1024, 512 };
#define PS5_EINVAL ((int)0x80020016)

DIR* __wrap_opendir(const char* path)
{
    Ps5Dir* d;
    int     fd;

    FIX_RO(path);
    {
        DIR* real = __real_opendir(path);

        if (real)
        {
            d = (Ps5Dir*)calloc(1, sizeof(Ps5Dir));
            if (!d)
            {
                __real_closedir(real);
                errno = ENOMEM;
                return NULL;
            }
            d->real = real;
            d->fd = -1;
            return (DIR*)d;
        }
    }
    fd = sceKernelOpen(path, O_RDONLY | O_DIRECTORY, 0);
    {
        static int s_logged = 0;
        if (s_logged < 12)
        {
            s_logged++;
            SH_DBG("[PATHS] opendir %s -> %s (%d)", path, fd >= 0 ? "ok" : "FAIL", fd);
        }
    }
    if (fd < 0)
    {
        errno = ENOENT;
        return NULL;
    }
    d = (Ps5Dir*)calloc(1, sizeof(Ps5Dir));
    if (!d)
    {
        sceKernelClose(fd);
        errno = ENOMEM;
        return NULL;
    }
    d->fd = fd;
    return (DIR*)d;
}

struct dirent* __wrap_readdir(DIR* dir)
{
    Ps5Dir* d = (Ps5Dir*)dir;

    if (!d)
        return NULL;
    if (d->real)
        return __real_readdir(d->real);
    for (;;)
    {
        struct dirent* e;

        if (d->pos >= d->len)
        {
            int n = -1;

            while (d->size_idx < (int)(sizeof(k_Ps5DentSizes) / sizeof(k_Ps5DentSizes[0])))
            {
                n = sceKernelGetdents(d->fd, d->buf, k_Ps5DentSizes[d->size_idx]);
                {
                    static int s_logged = 0;
                    if (s_logged < 8)
                    {
                        s_logged++;
                        SH_DBG("[PATHS] getdents fd %d size %d -> 0x%x", d->fd, k_Ps5DentSizes[d->size_idx], (unsigned)n);
                    }
                }
                if (n != PS5_EINVAL)
                    break;
                d->size_idx++;
            }
            if (n <= 0)
                return NULL;
            d->len = n;
            d->pos = 0;
        }
        e = (struct dirent*)(d->buf + d->pos);
        if (e->d_reclen == 0)
            return NULL;
        d->pos += e->d_reclen;
        if (e->d_fileno != 0)
            return e;
    }
}

int __wrap_closedir(DIR* dir)
{
    Ps5Dir* d = (Ps5Dir*)dir;

    if (!d)
        return -1;
    if (d->real)
        __real_closedir(d->real);
    else
        sceKernelClose(d->fd);
    free(d);
    return 0;
}

int __wrap_mkdir(const char* path, mode_t mode)
{
    FIX(path);
    return __real_mkdir(path, mode);
}

int __wrap_remove(const char* path)
{
    FIX(path);
    return __real_remove(path);
}

int __wrap_unlink(const char* path)
{
    FIX(path);
    return __real_unlink(path);
}

int __wrap_rmdir(const char* path)
{
    FIX(path);
    return __real_rmdir(path);
}

int __wrap_rename(const char* from, const char* to)
{
    char to_buf[1024];
    FIX(from);
    to = fix(to, to_buf, sizeof(to_buf));
    return __real_rename(from, to);
}

/* The sandbox lets the game read files under /app0 but not list its directories (opendir gives
 * EPERM), and the language packs are found by listing gamedata/lang. So the assets that travel
 * with the title (assets/manifest.txt names every file) are copied under /data/SilentHill on
 * start when they are missing; whatever is already there is left alone, so it stays editable. */
void ps5_install_assets(void)
{
    char  line[512];
    FILE* manifest;

    __real_mkdir("/data/SilentHill", 0777);
    manifest = __real_fopen("/app0/assets/manifest.txt", "r");
    if (!manifest)
        return;

    while (fgets(line, sizeof(line), manifest))
    {
        char        src[640], dst[640];
        struct stat st;
        size_t      len = strlen(line);
        char*       slash;
        FILE*       in;
        FILE*       out;

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0 || line[0] == '/' || strstr(line, ".."))
            continue;

        snprintf(src, sizeof(src), "%s%s", k_Ps5AssetRoot, line);
        snprintf(dst, sizeof(dst), "%s%s", g_Ps5Root, line);
        if (__real_stat(dst, &st) == 0)
            continue;

        for (slash = strchr(dst + strlen(g_Ps5Root), '/'); slash; slash = strchr(slash + 1, '/'))
        {
            *slash = '\0';
            __real_mkdir(dst, 0777);
            *slash = '/';
        }

        in = __real_fopen(src, "rb");
        if (!in)
            continue;
        out = __real_fopen(dst, "wb");
        if (out)
        {
            char   chunk[16384];
            size_t got;

            while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0)
                fwrite(chunk, 1, got, out);
            fclose(out);
        }
        fclose(in);
    }
    fclose(manifest);
}

char* __wrap_getcwd(char* buf, size_t size)
{
    size_t len = strlen(g_Ps5Root);

    if (!buf || size <= len)
        return NULL;
    memcpy(buf, g_Ps5Root, len + 1);
    if (len > 1)
        buf[len - 1] = '\0';
    return buf;
}

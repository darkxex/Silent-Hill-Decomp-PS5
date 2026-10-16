#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char* g_Ps5Root = "/data/SilentHill/";

FILE* __real_fopen(const char* path, const char* mode);
int __real_open(const char* path, int flags, ...);
int __real_stat(const char* path, struct stat* st);
int __real_lstat(const char* path, struct stat* st);
int __real_access(const char* path, int mode);
DIR* __real_opendir(const char* path);
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

#define FIX(p) char p##_buf[1024]; p = fix(p, p##_buf, sizeof(p##_buf))

FILE* __wrap_fopen(const char* path, const char* mode)
{
    FIX(path);
    return __real_fopen(path, mode);
}

int __wrap_open(const char* path, int flags, ...)
{
    FIX(path);
    mode_t mode = 0;
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
    FIX(path);
    return __real_stat(path, st);
}

int __wrap_lstat(const char* path, struct stat* st)
{
    FIX(path);
    return __real_lstat(path, st);
}

int __wrap_access(const char* path, int mode)
{
    FIX(path);
    return __real_access(path, mode);
}

DIR* __wrap_opendir(const char* path)
{
    FIX(path);
    return __real_opendir(path);
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

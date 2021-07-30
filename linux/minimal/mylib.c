//cc -fPIC -shared -o mylib.so mylib.c -ldl
//LD_PRELOAD=./mylib.so ./myprogram filename.txt
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

// Only works on Linux
char *recover_filename(FILE *f)
{
    char fd_path[256];
    int fd = fileno(f);
    sprintf(fd_path, "/proc/self/fd/%d", fd);
    char *filename = malloc(256);
    int n;
    if ((n = readlink(fd_path, filename, 255)) < 0)
        return NULL;
    filename[n] = '\0';
    return filename;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t (*lfread)(void *, size_t, size_t, FILE*) = dlsym(RTLD_NEXT, "fread");
    char *fname = recover_filename(stream);
    printf("Read from file %s\n", fname);
    free(fname);
    return lfread(ptr, size, nmemb, stream);
}

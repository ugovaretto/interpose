#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

ssize_t (*orig_read)(int, void*, size_t) = NULL;

ssize_t read(int fd, void *buf, size_t len)
{
    printf("read was called\n");
    if (!orig_read)
    {
        printf("orig_read was not initialized\n");
        return -1;
    }
  
    return orig_read(fd, buf, len);
}
  
static __attribute__((constructor)) void init_method2(void)
{
    printf("init_method2 was called\n");
    char sym;
    read(0, &sym, sizeof(sym));
}
  
static __attribute__((constructor)) void init_method(void)
{
    printf("init_method was called\n");
    orig_read = dlsym(RTLD_NEXT, "read");
    printf("read was initialized\n");
}
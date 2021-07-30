#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

extern ssize_t read(int, void*, size_t);

int main(int argc, char const *argv[]) {
    read(1,NULL, 0);
    /* code */
    return 0;
}

#include <stdio.h>
int main(int argc, char **argv)
{
    FILE *f;
    char buf[1024];
    f = fopen(argv[1], "rb");
    fread(buf, 1024, 1, f);
    return 0;
}
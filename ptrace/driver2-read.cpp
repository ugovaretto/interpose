#include <stdio.h>
#include <string.h>
int main() {
  const char* in = "in";
  FILE* f = fopen("in", "r");
  static char buf[] = {0,0,0,0,0,0}; 
  fread((void*) buf, 10, 1, f);
  fclose(f);
  fprintf(stderr, ">>> %s\n", buf);
  return 0;
}
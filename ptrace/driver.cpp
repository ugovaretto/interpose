#include <stdio.h>
#include <string.h>
int main() {
  FILE* f = fopen("out", "w");
  static const char* buf = "ciao";
  fwrite(buf, strlen(buf), 1, f);
  fclose(f);
  return 0;
}
#include <include/linux/kallsims.h>

static unsigned long *sys_call_table;
 
int init_module(void) {
  sys_call_table = (void *)kallsyms_lookup_name("sys_call_table");
 
  if (sys_call_table == NULL) {
    printk(KERN_ERR "Couldn't look up sys_call_table\n");
    return -1;
  }
 
  return 0;
}

int main(int argc, char const *argv[]) {
    /* code */
    return 0;
}

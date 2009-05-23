#include <stdio.h>
#include <patient0/mach_jump.h>

int main() {
  mach_jump_init();
  mach_jump_patch("open", &printf);
  printf("patched\n");
  open("open sez hello (%d)!\n\n", 1);
  return 0;
}

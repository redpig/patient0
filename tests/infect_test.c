#include <stdio.h>
#include <patient0/infect.h>
#include <patient0/spawn.h>

#include "dummy.h"

int main() {
  char *argv[] = {"/bin/sleep", "30", NULL};
  char *envp[] = {NULL};
  thread_act_t thread;
  mach_port_t port = MACH_PORT_NULL;
  pid_t pid = spawn("/bin/sleep", argv, envp, &port, -1);
  if (pid <= 0 && port == MACH_PORT_NULL) {
    printf("FAIL!\n");
    return 1;
  }
  /* No rush, let's assume 2 seconds is enough time to get our
   * process properly bootstrapped (execve, dyld, etc.
   */
  sleep(2);
  if (!infect(port, dummy_bundle, dummy_bundle_len, &thread)) {
    printf("FAILED TO INFECT!\n");
    return 1;
  }
  printf("Check if /tmp/dummy.bundle.out exists!\n");
  return 0;
}

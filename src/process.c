/* vim:tw=80:ts=2:et:sw=2
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <patient0/log.h>

size_t process_kernel_max() {
 size_t max = 0;
 size_t max_sz = sizeof(max);
 int mib[] = { CTL_KERN, KERN_MAXPROC };
 if (sysctl(mib, 2, &max, &max_sz, NULL, 0) == -1) {
   p0_logf(P0_ERR, "failed to get KERN_MAXPROC");
   return 0;
 }
 return max;
}


bool process_list(struct kinfo_proc **processes, size_t *count) {
  static const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
  size_t miblength = (sizeof(mib) / sizeof(*mib)) - 1;
  size_t length = process_kernel_max() * sizeof(struct kinfo_proc);

  /* Allocate the process list with the discovered size */
  *processes = malloc(length);
  if (*processes == NULL) {
    p0_logf(P0_ERR, "could not allocate enough memory for a process list");
    *count = 0;
    return false;
  }

  /* Populate the process list */
  *count = length;
  if (sysctl((int *)mib, 3, *processes, count, NULL, 0) == -1) {
    p0_logf(P0_ERR, "failed to get processes");
    return false;
  }
  p0_logf(P0_INFO, "received process list with count: %d", *count);
  if (*count > 0) {
    *count /= sizeof(struct kinfo_proc);
  }

  return true;
}

pid_t process_find(const char *prefix) {
  struct kinfo_proc *list = NULL, *cursor = NULL;
  size_t list_length = 0;
  size_t count = 0;
  size_t prefix_length = strlen(prefix);
  pid_t pid = -1;

  if (!process_list(&list, &list_length)) {
    p0_logf(P0_ERR, "failed to get process list");
    return -1;
  }

  /* Make sure the given prefix string isn't longer than the
   * maximum size allowed
   */
  if (prefix_length > MAXCOMLEN) {
    prefix_length = MAXCOMLEN;
  }

  /* Walk the list until we match the prefix */
  p0_logf(P0_INFO, "scanning process list: %d\n", list_length);
  for (cursor = list ; count < list_length; ++count, ++cursor) {
    p0_logf(P0_INFO, "process: %s\n", cursor->kp_proc.p_comm);
    if (strncmp(cursor->kp_proc.p_comm, prefix, prefix_length) == 0) {
      pid = cursor->kp_proc.p_pid;
      break;
    }
  }

  p0_logf(P0_INFO, "done with process list\n");
  /* No luck */
  free(list);
  return pid;
}

#ifdef PROCESS_MAIN
int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage:\n%s command_name\nE.g, %s Dock\n", argv[0], argv[0]);
    return -1;
  }
  printf("pid is %d\n", process_find(argv[1]));
  return 0;
}
#endif

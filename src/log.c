/* vim:tw=80:ts=2:et:sw=2
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <patient0/log.h>

#ifdef P0_LOG_FILE
FILE *p0_log_file = NULL;
__attribute__((constructor)) static void p0_log_init() {
  pid_t pid = getpid();
  char fname[256] = {0};
  snprintf(fname, 256, "%s.%d.log", P0_LOG_FILE, pid);
  p0_log_file = fopen(fname, "a");
}
#endif

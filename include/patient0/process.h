/* spawn.h
 * Public interfaces for process spawning with mach task ports
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#ifndef PATIENT0_PROCESS_H_
#define PATIENT0_PROCESS_H_

#include <stdbool.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <unistd.h>

size_t process_kernel_max();
pid_t process_find(const char *prefix);
bool process_list(struct kinfo_proc **processes, size_t *count);
#endif  /* PATIENT0_PROCESS_H_ */

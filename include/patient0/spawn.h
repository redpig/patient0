/* spawn.h
 * Public interfaces for process spawning with mach task ports
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#ifndef PATIENT0_SPAWN_H_
#define PATIENT0_SPAWN_H_

#include <stdbool.h>
#include <mach/mach_types.h>
#include <unistd.h>

/* spawn
 *
 * Launches the executable at 'path' with the given argv and envp. It will
 * return that process' mach task port in 'taskport'.  If a positive pid
 * value is set, that pid will receive the SIGKILL signal just before execv*()
 * is called.  This allows for quick replacement for auto-spawned processes,
 * like Dock.
 */
pid_t spawn(const char *path,
            char **argv,
            char **envp,
            mach_port_t *taskport,
            pid_t pid);

#endif  /* PATIENT0_SPAWN_H_ */

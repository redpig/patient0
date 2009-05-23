/*
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/i386/thread_status.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/socket.h>

#include <patient0/log.h>

void runtime_deadlock() {
  pthread_mutex_t l = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&l);
  pthread_mutex_lock(&l);
}

/* mach_jump.h
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_INFECT_H_
#define PATIENT0_INFECT_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <mach/mach_types.h>

bool infect(mach_port_t task,
            const unsigned char *bundle,
            size_t size,
            thread_act_t *thread);

#endif  /* PATIENT0_INFECT_H_ */

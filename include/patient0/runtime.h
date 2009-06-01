/* runtime.h
 * Utilities for use during patient0 & pathogen runtime.
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_RUNTIME_H_
#define PATIENT0_RUNTIME_H_
#include <stdint.h>
#include <sys/types.h>

#define PATIENT0_PAYLOAD_SIZE(__code, __size) \
  (*((uint32_t *)(__code + __size - sizeof(uint32_t))))
#define PATIENT0_PAYLOAD(__code, __size) \
  ((char *) \
   (!__size ? \
    0 : \
    (__code + __size) - \
    ((PATIENT0_PAYLOAD_SIZE(__code, __size)) + sizeof(uint32_t))))

void runtime_deadlock();
void runtime_terminate();

#endif  /* PATIENT0_RUNTIME_H_ */

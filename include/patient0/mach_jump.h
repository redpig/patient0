/* mach_jump.h
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_MACH_JUMP_H_
#define PATIENT0_MACH_JUMP_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

bool mach_jump_init();
bool mach_jump_patch(const char *symbol, void *replacement_fn);
bool mach_jump_unpatch(const char *symbol);
bool mach_jump_framework_patch(const char *framework,
                               const char *symbol,
                               void *replacement);
bool mach_jump_framework_unpatch(const char *framework,
                                 const char *symbol,
                                 void *replacement);

#endif  /* PATIENT0_MACH_JUMP_H_ */

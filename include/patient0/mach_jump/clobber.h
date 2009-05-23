/*
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_CLOBBER_H_
#define PATIENT0_CLOBBER_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

int clobber_function_by_symbol(const char *sym, intptr_t ptr_to_addrptr);

#endif  /* PATIENT0_CLOBBER_H_ */

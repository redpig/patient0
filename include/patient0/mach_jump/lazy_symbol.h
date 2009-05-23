/*
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_LAZY_SYMBOL_H_
#define PATIENT0_LAZY_SYMBOL_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

bool lazy_symbol_init();
intptr_t lazy_symbol_stub(const char *symbol);

#endif  /* PATIENT0_LAZY_SYMBOL_H_ */

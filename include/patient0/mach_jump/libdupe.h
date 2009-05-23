/* DO NOT USE. UNFINISHED.
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_LIBDUPE_H_
#define PATIENT0_LIBDUPE_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct libdupe_entry {
  uint8_t *base;
  uint32_t size;
  uint8_t *original_base;
} libdupe_entry_t;

bool libdupe_dupe(const char *symbol, libdupe_entry_t *entry);

#endif  /* PATIENT0_LIBDUPE_H_ */
